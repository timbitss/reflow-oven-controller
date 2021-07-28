/**
 * @file log.h
 * @author Timothy Nguyen
 * @brief Console logging module.
 * @version 0.1
 * @date 2021-07-16
 */

#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <sys/queue.h>
#include <stdlib.h>

#include "log.h"
#include "common.h"
#include "stm32l4xx_hal.h"
#include "printf.h"
#include "cmd.h"

////////////////////////////////////////////////////////////////////////////////
// Common macros
////////////////////////////////////////////////////////////////////////////////

/* Logging names */
#define LOG_LEVEL_NAMES "OFF, ERROR, WARNING, INFO, DEBUG, VERBOSE"
#define LOG_LEVEL_NAMES_CSV "OFF", "ERROR", "WARNING", "INFO", "DEBUG", "VERBOSE"

/* Number of tags to be cached. Must be 2**n - 1, n >= 2. */
#define TAG_CACHE_SIZE 31

////////////////////////////////////////////////////////////////////////////////
// Type definitions
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Linked list tag node.
 *
 * Tag entries in their linked list form are considered "uncached".
 * Since traversing the list each time a log message is slow,
 * each tag entry is replicated in a binary min-heap cache.
 */
typedef struct Log_entry
{
    SLIST_ENTRY(Log_entry)
    entries;           // Required structure connecting elements in list.
    log_level_t level; // Module's logging level.
    char tag[10];      // Unique module tag.
} Log_entry;

/**
 * @brief Cached log entry.
 *
 * Tag entries are cached in a binary min-heap for faster access.
 */
typedef struct
{
    uint32_t generation : 29;
    uint32_t level : 3; // Module's logging level.
    const char *tag;    // Tag unique to module
} Log_cached_entry;

/**
 * @brief View state of cache.
 */
typedef struct
{
    uint32_t max_generation;                // Highest generation value currently in heap.
    uint32_t entry_count;                   // Number of entries in cache.
    Log_cached_entry cache[TAG_CACHE_SIZE]; // Binary min-heap acting as a cache.
} Log_cache_state_t;

/**
 * @brief Structure named Log_head_t containing a pointer to head log_entry node.
 */
SLIST_HEAD(Log_head_t, Log_entry);

////////////////////////////////////////////////////////////////////////////////
// Private (static) function declarations
////////////////////////////////////////////////////////////////////////////////

/* Command callback functions */
static uint32_t cmd_log_status(uint32_t argc, const char **argv); // Get log levels callback.
static uint32_t cmd_log_set(uint32_t argc, const char **argv);    // Set log level callback.

static inline void log_level_set(const char *tag, log_level_t level); // Set tag's log level.

static const char *log_level_str(int32_t level);      // Convert log level from integer to string.
static int32_t log_level_int(const char *level_name); // Convert log level from string to integer.

static inline log_level_t get_log_level(const char *tag);                       // Get log level, either from heap (cache) or linked list (uncached).
static inline bool get_cached_log_level(const char *tag, log_level_t *level);   // Get cached log level.
static inline bool get_uncached_log_level(const char *tag, log_level_t *level); // Get uncached log level.

static inline void log_add_cache(const char *tag, log_level_t log_level); // Add tag and log level to cache.

static inline void heap_bubble_down(int index);       // Heapify min-heap.
static inline void heap_swap(uint32_t i, uint32_t j); // Swap heap array elements.

////////////////////////////////////////////////////////////////////////////////
// Private (static) variables
////////////////////////////////////////////////////////////////////////////////

/* Log level names */
static const char *log_level_names[] = {
    LOG_LEVEL_NAMES_CSV};

/* Log command information. */
static cmd_cmd_info log_cmds[] = {
    {.cmd_name = "status",
     .cb = cmd_log_status,
     .help = "Display log levels.\r\nPossible log levels: " LOG_LEVEL_NAMES},
    {.cmd_name = "set",
     .cb = cmd_log_set,
     .help = "Set tag's log level, usage: log set <tag> <level>.\r\nPossible log levels: " LOG_LEVEL_NAMES}};

/* Log module client info */
static cmd_client_info log_client_info =
    {
        .client_name = "log",
        .num_cmds = 2,
        .cmds = log_cmds,
        .num_u16_pms = 0,
        .u16_pms = NULL,
        .u16_pm_names = NULL};

/* Unique tag for logging module. */
static const char *TAG = "LOG";

/* Declare a Log_head_t object containing a pointer to first log_tag_entry node. */
static struct Log_head_t log_head;

/* Cache of tags and their log levels */
static Log_cache_state_t cache_state;

////////////////////////////////////////////////////////////////////////////////
// Public (global) variables and externs
////////////////////////////////////////////////////////////////////////////////

/* Logging state */
bool _log_active = true;

/*
 * @brief Global log level.
 *
 * Local tag log levels that are set with log_level_set()
 * will override this variable.
 */
int32_t _global_log_level = LOG_DEFAULT;

////////////////////////////////////////////////////////////////////////////////
// Public (global) functions
////////////////////////////////////////////////////////////////////////////////

mod_err_t log_init(void)
{
    SLIST_INIT(&log_head); // Initialize linked list by setting head pointer to NULL.
    LOGI(TAG, "Initialized log module");
    return cmd_register(&log_client_info);
}

bool log_toggle(void)
{
    _log_active = _log_active ? false : true;
    return _log_active;
}

bool log_is_active(void)
{
    return _log_active;
}

void log_printf(const char *tag, log_level_t level, const char *fmt, ...)
{
    log_level_t tag_level = get_log_level(tag);
    if (level > tag_level)
    {
        return;
    }
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

/**
 * @brief Convert integer log level to a string.
 *
 * @param level The log level as an integer.
 *
 * @return Log level as a string. Otherwise, "INVALID".
 */
static const char *log_level_str(int32_t level)
{
    if (level < ARRAY_SIZE(log_level_names))
    {
        return log_level_names[level];
    }
    return "INVALID";
}
/**
 * @brief Convert log level string to an int.
 *
 * @param level_name The log level as a string.
 *
 * @return Log level as an int, or -1 on error.
 */
static int32_t log_level_int(const char *level_name)
{
    for (uint8_t level = 0; level < ARRAY_SIZE(log_level_names); level++)
    {
        if (strcasecmp(level_name, log_level_names[level]) == 0)
        {
            return level;
        }
    }

    return -1; // Log level not found.
}

////////////////////////////////////////////////////////////////////////////////
// Private (static) functions
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Display log level to user.
 *
 * @param argc Number of arguments.
 * @param argv Argument values.
 *
 * @return 0 if successful, 1 otherwise.
 *
 * Log levels include global log level and individual tag log levels that override global level.
 */
static uint32_t cmd_log_status(uint32_t argc, const char **argv)
{
    LOG("Global log level: (%s)\r\n", log_level_str(_global_log_level));

    if (!SLIST_EMPTY(&log_head))
    {
        Log_entry *p = NULL;
        SLIST_FOREACH(p, &log_head, entries)
        {
            LOG("%s log level: (%s)\r\n", p->tag, log_level_str(p->level));
        }
    }

    return 0;
}

/**
 * @brief Log set level command.
 *
 * @param argc Number of arguments.
 * @param argv Argument values.
 *
 * @return 0 if successful, 1 otherwise.
 *
 * TTYS command format: > log set <tag> <level>.
 */
static uint32_t cmd_log_set(uint32_t argc, const char **argv)
{
    if (argc != 2)
    {
        LOGW(TAG, "Missing log level arguments");
        return 1; // Should include only 1 argument.
    }
    else
    {
        int32_t new_log_level = log_level_int(argv[1]);
        if (new_log_level == -1)
        {
            LOGW(TAG, "Log level (%s) not recognized", argv[1]);
            return 1;
        }
        else
        { // tag , level
            log_level_set(argv[0], new_log_level);
            return 0;
        }
    }
}

/**
 * @brief Set log level.
 * 
 * @param tag Tag unique to module or wild-card "*" if referring to all modules.
 * @param level Desired log level.
 *
 * @note Wild-card tag resets log level of all tags to given value.
 */
static inline void log_level_set(const char *tag, log_level_t level)
{
    Log_entry *p = NULL;

    /* Set global log level and delete linked list containing tag entries. */
    if (strcmp("*", tag) == 0)
    {
        _global_log_level = level;

        LOGI(TAG, "Clearing list and cache");
        while (!SLIST_EMPTY(&log_head))
        {
            p = SLIST_FIRST(&log_head);
            SLIST_REMOVE_HEAD(&log_head, entries);
            free(p);
        }

        cache_state.entry_count = 0;
        cache_state.max_generation = 0;

        LOG("Global log level set to (%s)\r\n", log_level_str(_global_log_level));
        return;
    }

    /* Check if tag is already saved in linked list. */
    SLIST_FOREACH(p, &log_head, entries)
    {
        if (strcmp(p->tag, tag) == 0)
        {
            p->level = level;
            LOG("%s log level set to (%s)\r\n", p->tag, log_level_str(p->level));
            break;
        }
    }

    /* Tag not found in linked list, add new entry. */
    if (p == NULL)
    {
        Log_entry *new_entry = (Log_entry *)malloc(sizeof(Log_entry));
        if (new_entry == NULL)
        {
            LOGW(TAG, "Insufficient heap memory for new list entry.");
            return;
        }
        new_entry->level = level;
        strncpy(new_entry->tag, tag, sizeof(new_entry->tag));
        SLIST_INSERT_HEAD(&log_head, new_entry, entries);
        LOG("Added tag (%s) to list with level (%s)\r\n", new_entry->tag, log_level_str(new_entry->level));
    }

    /* Update entry in cache if it exists.
     * search in the cache and update the entry it if exists */
    for (uint32_t i = 0; i < cache_state.entry_count; ++i)
    {
        if (strcmp(cache_state.cache[i].tag, tag) == 0)
        {
            cache_state.cache[i].level = level;
            break;
        }
    }

    return;
}

/**
 * Get log level.
 *
 * @param tag Tag to find level of.
 *
 * @return Tag's log level or global log level if not found.
 */
static inline log_level_t get_log_level(const char *tag)
{
    log_level_t tag_lvl = 0;
    if (!get_cached_log_level(tag, &tag_lvl))
    {
        if (!get_uncached_log_level(tag, &tag_lvl))
        {
            /* Log level not found, default to global log level. */
            tag_lvl = _global_log_level;
        }

        /* Add to cache for faster access */
        log_add_cache(tag, tag_lvl);
    }

    return tag_lvl;
}

/**
 * @brief Get the cached log level corresponding to tag.
 * 
 * @param[in] tag Tag to find cached level of.
 * @param[out] level Log level corresponding to tag.
 * @return true Level found in cache.
 *         false Level not found in cache.
 */
static inline bool get_cached_log_level(const char *tag, log_level_t *level)
{

    /* Assumes tag stored as static variable */
    uint32_t i;
    for (i = 0; i < cache_state.entry_count; ++i)
    {
        if (cache_state.cache[i].tag == tag)
        {
            break;
        }
    }

    /* Could not find log level */
    if (i == cache_state.entry_count)
    {
        return false;
    }

    /* Return level */
    *level = (log_level_t)cache_state.cache[i].level;

    /* If cache is full, increment generation with each cache hit and heapify */
    if (cache_state.entry_count == TAG_CACHE_SIZE)
    {
        cache_state.cache[i].generation = cache_state.max_generation++;
        heap_bubble_down(i);
    }

    return true;
}

/**
 * @brief Get uncached log level.
 *
 * @param[in] tag Unique module tag.
 * @param[out] level Log level corresponding to tag.
 *
 * @return true if log level found, false otherwise.
 */
static inline bool get_uncached_log_level(const char *tag, log_level_t *level)
{
    Log_entry *p = NULL;

    SLIST_FOREACH(p, &log_head, entries)
    {
        if (strcmp(p->tag, tag) == 0)
        {
            *level = p->level;
            return true;
        }
    }

    return false;
}

/**
 * @brief Add tag and its log level to cache.
 *
 * @param tag Unique tag corresponding to module
 * @param log_level Module's log level.
 *
 * Cache is implemented using a binary min-heap.
 */
static inline void log_add_cache(const char *tag, log_level_t log_level)
{
    uint32_t generation = cache_state.max_generation++;

    /* No need to sort since min-heap. */
    if (cache_state.entry_count < TAG_CACHE_SIZE)
    {
        cache_state.cache[cache_state.entry_count] = (Log_cached_entry){
            .generation = generation,
            .level = log_level,
            .tag = tag};
        ++cache_state.entry_count;
        return;
    }

    // Cache is full, replace first element
    // and do bubble-down sorting to restore
    // binary min-heap.
    cache_state.cache[0] = (Log_cached_entry){
        .tag = tag,
        .level = log_level,
        .generation = generation};
    heap_bubble_down(0);
}

static inline void heap_bubble_down(int index)
{
    while (index < TAG_CACHE_SIZE / 2)
    {
        uint32_t left_index = index * 2 + 1;
        uint32_t right_index = left_index + 1;
        uint32_t next = (cache_state.cache[left_index].generation < cache_state.cache[right_index].generation) ? left_index : right_index;
        heap_swap(index, next); // cache[index] always greater than cache[next]
        index = next;
    }
}

static inline void heap_swap(uint32_t i, uint32_t j)
{
    Log_cached_entry tmp = cache_state.cache[i];
    cache_state.cache[i] = cache_state.cache[j];
    cache_state.cache[j] = tmp;
}

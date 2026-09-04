/* HouseAlmanac - A service that calculates Almanac data
 *
 * Copyright 2026, Pascal Martin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * housealmanac_cache.c -- Reuse calculated sunraise and sunset times.
 *
 * void housealmanac_cache_location (double latitude, double longitude);
 *
 *    Set the current location. Not sunset or sunrise can be calculated
 *    until these have been provided. The location can be changed at will.
 *
 * const char *housealmanac_cache_refresh (time_t now);
 *
 *    Make sure that the cache is not stale. This function must be called
 *    by the application before calling any combination of the other functions.
 *    Return null on success, an error message otherwise. The cache is still
 *    stale when an error was returned.
 *
 * void housealmanac_cache_yesterday (time_t *rise, time_t *set);
 * void housealmanac_cache_today (time_t *rise, time_t *set);
 * void housealmanac_cache_tomorrow (time_t *rise, time_t *set);
 *
 *    Get the sunrise and sunset times for yesterday, today or tomorrow.
 *    The cache must have been refreshed before these calls.
 *
 * void housealmanac_cache_tonight (time_t now, time_t *set, time_t *rise);
 *
 *    Get the sunset and sunrise times for the upcoming night.
 *    The cache must have been refreshed before this call.
 *
 * time_t housealmanac_cache_updated (void);
 *
 *    Return the time when the cache data was last updated.
 *
 * const char *housealmanac_cache_origin (void);
 *
 *    Return a static string describing the origin of the data.
 *
 * void housealmanac_cache_background (time_t now);
 *
 *    A periodic function to maintain the cache data up-to-date.
 */

#include <time.h>

#include "housealmanac_cache.h"
#include "housealmanac_calculate.h"

struct DayCache {
    struct tm date;
    time_t sunrise;
    time_t sunset;
};

static struct DayCache AlmanacYesterday = {0};
static struct DayCache AlmanacToday     = {0};
static struct DayCache AlmanacTomorrow  = {0};

static time_t AlmanacUpdated = 0;

static int AlmanacHasLocation = 0;
static double AlmanacLatitude = 0.0;
static double AlmanacLongitude = 0.0;

static const struct tm AlmanacInvalid = {0};

void housealmanac_cache_location (double latitude, double longitude) {

   AlmanacLatitude = latitude;
   AlmanacLongitude = longitude;
   AlmanacHasLocation = 1;
}

static const char *housealmanac_cache_calculate (time_t now,
                                                 const struct tm *day,
                                                 struct DayCache *cache) {

   const char *error = housealmanac_calculate (day,
                                               AlmanacLatitude,
                                               AlmanacLongitude,
                                               &(cache->sunrise),
                                               &(cache->sunset));
   if (error) {
       cache->date = AlmanacInvalid;
       return error;
   }
   cache->date = *day;
   return 0;
}

static int housealmanac_cache_same_day (const struct tm *day,
                                        const struct DayCache *cache) {

   return (day->tm_yday == cache->date.tm_yday) &&
          (day->tm_year == cache->date.tm_year) &&
          (day->tm_gmtoff == cache->date.tm_gmtoff);
}

const char *housealmanac_cache_refresh (time_t now) {

    if (! AlmanacHasLocation) return "unknown location";

    const char *error;
    struct tm today = *localtime (&now);

    if (housealmanac_cache_same_day (&today, &AlmanacToday)) return 0; // OK

    if (housealmanac_cache_same_day (&today, &AlmanacTomorrow)) {
        // The day changed: shift the cache entry to avoid recomputing what
        // is already known.
        AlmanacYesterday = AlmanacToday;
        AlmanacToday = AlmanacTomorrow;
    }

    time_t tomorrow = now + (24*60*60);
    struct tm date = *localtime (&tomorrow);
    error = housealmanac_cache_calculate (tomorrow, &date, &AlmanacTomorrow);
    if (error) return error;

    time_t yesterday = now - (24*60*60);
    date = *localtime (&yesterday);
    if (!housealmanac_cache_same_day (&date, &AlmanacYesterday)) {
        error = housealmanac_cache_calculate
                    (yesterday, &date, &AlmanacYesterday);
        if (error) return error;
    }

    if (!housealmanac_cache_same_day (&today, &AlmanacToday)) {
        error = housealmanac_cache_calculate (now, &today, &AlmanacToday);
        if (error) return error;
    }
    AlmanacUpdated = now;
    return 0;
}

void housealmanac_cache_yesterday (time_t *rise, time_t *set) {
    *rise = AlmanacYesterday.sunrise;
    *set = AlmanacYesterday.sunset;
}

void housealmanac_cache_today (time_t *rise, time_t *set) {
    *rise = AlmanacToday.sunrise;
    *set = AlmanacToday.sunset;
}

void housealmanac_cache_tomorrow (time_t *rise, time_t *set) {
    *rise = AlmanacTomorrow.sunrise;
    *set = AlmanacTomorrow.sunset;
}

void housealmanac_cache_tonight (time_t now, time_t *set, time_t *rise) {

    if (now > AlmanacToday.sunrise) {
       // That night is over, look for the next night.
       *set = AlmanacToday.sunset;
       *rise = AlmanacTomorrow.sunrise;
    } else {
       *set = AlmanacYesterday.sunset;
       *rise = AlmanacToday.sunrise;
    }
}

time_t housealmanac_cache_updated (void) {
    return AlmanacUpdated;
}

const char *housealmanac_cache_origin (void) {
    return "local calculation";
}

void housealmanac_cache_background (time_t now) {

    static time_t LastCall = 0;

    if (LastCall && (now % 60)) return;
    LastCall = now;

    housealmanac_cache_refresh (now);
}


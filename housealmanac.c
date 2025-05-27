/* housealmanac - A simple home web server for rough Almanac data.
 *
 * Copyright 2025, Pascal Martin
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
 *
 * housealmanac.c - Generate almanac data from fixed configuration.
 *
 * SYNOPSYS:
 *
 * Almanac data (such as sunrise and sunset times) are typically provided
 * by weather services. This service provides a rough approximation for
 * that same data using a local static configuration. This serves two
 * purposes:
 * - as a simulation and test tool,
 * - as a reasonable fallback method when the Internet is not available.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "echttp.h"
#include "echttp_cors.h"
#include "echttp_json.h"
#include "echttp_static.h"
#include "houseportalclient.h"
#include "housediscover.h"
#include "houselog.h"
#include "houseconfig.h"
#include "housedepositor.h"

static int  UseHousePortal = 0;

#define DEBUG if (echttp_isdebug()) printf

static int DaysPerMonth[12] = {
    31, // January.
    28, // February (most of the time)
    31, // March
    30, // April
    31, // May
    30, // June
    31, // July
    31, // August
    30, // September
    31, // October
    30, // November
    31  // December
};

typedef struct {
    char month;
    char day;
    char hour;
    char minute;
} DayTimePoint;

typedef struct {
    DayTimePoint sunrises[12];
    DayTimePoint sunsets[12];
    DayTimePoint dst[2];
} AlmanacData;

static AlmanacData AlmanacDb;

static int AlmanacIsConfigured = 0;

static void housealmanac_printentry (const char *label, int index, DayTimePoint *data) {
    DEBUG ("%s[%d]: month %d, day %d, hour %d, minute %d\n", label, index, data->month, data->day, data->hour, data->minute);
}

static const char *housealmanac_refresh (void) {

    DEBUG ("Refreshing the almanac database\n");

    int sunrises = houseconfig_array (0, ".almanac.sunrise");
    if (sunrises < 0) return "cannot find sunrises array";

    if (houseconfig_array_length (sunrises) != 12)
        return "not a valid sunrises array";

    int sunsets = houseconfig_array (0, ".almanac.sunset");
    if (sunsets < 0) return "cannot find sunsets array";

    if (houseconfig_array_length (sunrises) != 12)
        return "not a valid sunrises array";

    int dst = houseconfig_array (0, ".almanac.dst");
    if (dst < 0) return "cannot find dst array";

    if (houseconfig_array_length (dst) != 2)
        return "not a valid dst array";

    int i;
    for (i = 0; i < 2; ++i) {
        char path[128];
        snprintf (path, sizeof(path), "[%d]", i);
        const char *dstdate = houseconfig_string (dst, path);
        if (dstdate) {
            AlmanacDb.dst[i].month = atoi(dstdate);
            AlmanacDb.dst[i].day = 15; // Arbitrary default.
            const char *slash = strchr (dstdate, '/');
            if (slash) AlmanacDb.dst[i].day = atoi(slash+1);
            AlmanacDb.dst[i].hour = 2;
            AlmanacDb.dst[i].minute = 0;
            if (echttp_isdebug())
                housealmanac_printentry ("dst", i, AlmanacDb.dst+i);
        }
    }

    for (i = 0; i < 12; ++i) {
        char path[128];
        snprintf (path, sizeof(path), "[%d]", i);
        const char *daytime = houseconfig_string (sunsets, path);
        if (daytime) {
            AlmanacDb.sunsets[i].month = i+1;
            AlmanacDb.sunsets[i].day = 15; // Implicit.
            AlmanacDb.sunsets[i].hour = atoi(daytime);
            AlmanacDb.sunsets[i].minute = 0;
            const char *column = strchr (daytime, ':');
            if (column) AlmanacDb.sunsets[i].minute = atoi(column+1);
            if (echttp_isdebug())
                housealmanac_printentry ("sunsets", i, AlmanacDb.sunsets+i);
        }
    }

    for (i = 0; i < 12; ++i) {
        char path[128];
        snprintf (path, sizeof(path), "[%d]", i);
        const char *daytime = houseconfig_string (sunrises, path);
        if (daytime) {
            AlmanacDb.sunrises[i].month = i+1;
            AlmanacDb.sunrises[i].day = 15; // Implicit.
            AlmanacDb.sunrises[i].hour = atoi(daytime);
            AlmanacDb.sunrises[i].minute = 0;
            const char *column = strchr (daytime, ':');
            if (column) AlmanacDb.sunrises[i].minute = atoi(column+1);
            if (echttp_isdebug())
                housealmanac_printentry ("sunrises", i, AlmanacDb.sunrises+i);
        }
    }
    AlmanacIsConfigured = 1;
    return 0;
}

static int housealmanac_before (DayTimePoint *dst, int month, int day) {
    int dstmonth = dst->month - 1;
    if (month < dstmonth) return 1;
    if (month > dstmonth) return 0;
    if (day < dst->day) return 1;
    return 0;
}

static void housealmanac_estimate (DayTimePoint monthly[], struct tm *now) {

    int month = now->tm_mon;
    int day = now->tm_mday;

    now->tm_sec = 0;

    if (day == 15) {
        now->tm_hour = monthly[month].hour;
        now->tm_min = monthly[month].minute;
        return;
    }

    int m1c, m2c; // Calendar month: 0 to 11.
    int m1r, m2r; // Month relative to this year: -1 to 12.
    if (day > 15) {
        m1r = m1c = month;
        if (month >= 11) {
            m2c = 0;
            m2r = 12; // Following year.
        } else {
            m2r = m2c = month+1;
        }
    } else {
        if (month > 0) {
            m1r = m1c = month-1;
        } else {
            m1c = 11;
            m1r = -1; // Previous year.
        }
        m2r = m2c = month;
    }
    int time1 = (((int)monthly[m1c].hour) * 60) + monthly[m1c].minute;
    int time2 = (((int)monthly[m2c].hour) * 60) + monthly[m2c].minute;

    // Adjust the reference times if they go across a dst change.
    int i;
    for (i = 0; i < 2; ++i) {
        if (housealmanac_before (AlmanacDb.dst+i, m1r, 15) !=
                housealmanac_before (AlmanacDb.dst+i, m2r, 15)) {
            DEBUG ("Interval [%d/15, %d/15] ([%d/15, %d/15]) crosses DST change on %d/%02d\n", m1c+1, m2c+1, m1r, m2r, AlmanacDb.dst[i].month, AlmanacDb.dst[i].day);
            if (housealmanac_before (AlmanacDb.dst+i, month, day)) {
                DEBUG ("Day %d/%02d is before the DST change on %d/%02d\n", month+1, day, AlmanacDb.dst[i].month, AlmanacDb.dst[i].day);
                time2 += (i == 0)?-60:60;
            } else {
                DEBUG ("Day %d/%02d is after the DST change on %d/%02d\n", month+1, day, AlmanacDb.dst[i].month, AlmanacDb.dst[i].day);
                time1 += (i == 0)?60:-60;
            }
            break;
        }
    }
    // Now use linear regression. Approximate a month to 30 days.
    // Because we use integers, don't use divisions except at the very end.
    int a = time2 - time1;
    int b = (30 * time1) - (a * ((m1r * 30) + 15));
    int result = ((a * ((month * 30) + (day-1))) + b) / 30;

    DEBUG ("day = %d/%02d, time1 = %d/15 %d:%02d, time2 = %d/15 %d:%02d, a = %d, b = %d, result = %d:%02d\n", month+1, day, m1c+1, time1/60, time1%60, m2c+1, time2/60, time2%60, a, b, result/60, result%60);

    now->tm_hour = result / 60;
    now->tm_min = result % 60;
}

static const char *housealmanac_timezone (void) {

    static char HouseTimeZone[256] = "";

    if (HouseTimeZone[0] == 0) {
        FILE *f = fopen ("/etc/timezone", "r");
        if (!f) exit(1);
        fgets (HouseTimeZone, sizeof(HouseTimeZone), f);
        fclose (f);
        char *eol = strchr (HouseTimeZone, '\n');
        if (eol) *eol = 0;
        DEBUG ("Obtained house timezone: %s\n", HouseTimeZone);
    }
    return HouseTimeZone;
}

static const char *housealmanac_export (ParserContext context) {

    static char buffer[65537];

    const char *error = echttp_json_export (context, buffer, sizeof(buffer));
    if (error) {
        echttp_error (500, error);
        return "";
    }
    echttp_content_type_json ();
    return buffer;
}

static void housealmanac_add_datetime
               (ParserContext context, int parent, struct tm *t) {

    char ascii[32];
    snprintf (ascii, sizeof(ascii), "%02d/%02d %02d:%02d",
              t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min);
    echttp_json_add_string (context, parent, 0, ascii);
}

static const char *housealmanac_selftest (const char *method, const char *uri,
                                          const char *data, int length) {

    ParserToken token[1024];
    char pool[65537];

    if (! AlmanacIsConfigured) {
        echttp_error (500, "Service initializing");
        return "";
    }

    ParserContext context = echttp_json_start (token, 1024, pool, 65537);
    int root = echttp_json_add_object (context, 0, 0);
    echttp_json_add_string (context, root, "host", houselog_host());
    echttp_json_add_string (context, root, "proxy", houseportal_server());
    echttp_json_add_integer (context, root, "timestamp", (long long)time(0));
    int top = echttp_json_add_object (context, root, "almanac");

    echttp_json_add_integer (context, top, "priority", 1);

    time_t now = time(0);
    struct tm today = *localtime (&now);

    int sunrise = echttp_json_add_array (context, top, "sunrise");
    int month;
    for (month = 0; month < 12; ++month) {
        int day;
        int count = DaysPerMonth[month];
        today.tm_mon = month;
        for (day = 1; day <= count; ++day) {
            today.tm_mday = day;
            housealmanac_estimate (AlmanacDb.sunrises, &today);
            housealmanac_add_datetime (context, sunrise, &today);
        }
    }
    int sunset = echttp_json_add_array (context, top, "sunset");
    for (month = 0; month < 12; ++month) {
        int day;
        int count = DaysPerMonth[month];
        today.tm_mon = month;
        for (day = 1; day <= count; ++day) {
            today.tm_mday = day;
            housealmanac_estimate (AlmanacDb.sunsets, &today);
            housealmanac_add_datetime (context, sunset, &today);
        }
    }

    return housealmanac_export (context);
}

static int housealmanac_header (time_t now, ParserContext context) {

    int root = echttp_json_add_object (context, 0, 0);
    echttp_json_add_string (context, root, "host", houselog_host());
    echttp_json_add_string (context, root, "proxy", houseportal_server());
    echttp_json_add_integer (context, root, "timestamp", (long long)now);

    // Add the location information that we know about.
    int loc = echttp_json_add_object (context, root, "location");
    echttp_json_add_string (context, loc, "timezone", housealmanac_timezone());

    int top = echttp_json_add_object (context, root, "almanac");
    echttp_json_add_integer (context, top, "priority", 1);
    echttp_json_add_integer (context, top, "updated", now); // Just estimated.

    return top;
}

static const char *housealmanac_tonight (const char *method, const char *uri,
                                         const char *data, int length) {
    ParserToken token[1024];
    char pool[65537];

    if (! AlmanacIsConfigured) {
        echttp_error (500, "Service initializing");
        return "";
    }

    time_t now = time(0);

    ParserContext context = echttp_json_start (token, 1024, pool, 65537);

    int top = housealmanac_header (now, context);

    // Estimate today's sunrise:
    // - if in the past or present, then return today's sunset and tomorrow's
    //   sunrise.
    // - if in the future, return yesterday's sunset and today's sunrise.
    //
    struct tm today = *localtime (&now);
    housealmanac_estimate (AlmanacDb.sunrises, &today);
    time_t sunrise = mktime (&today);

    if (sunrise < now) {
       // Use today's sunset and tomorrow's sunrise.
       housealmanac_estimate (AlmanacDb.sunsets, &today);
       echttp_json_add_integer (context, top, "sunset", mktime (&today));

       now += (24*60*60);
       struct tm tomorrow = *localtime (&now);
       housealmanac_estimate (AlmanacDb.sunrises, &tomorrow);
       echttp_json_add_integer (context, top, "sunrise", mktime (&tomorrow));
    } else {
       // Use yesterday's sunset and today's sunrise.
       now -= (24*60*60);
       struct tm yesterday = *localtime (&now);
       housealmanac_estimate (AlmanacDb.sunsets, &yesterday);
       echttp_json_add_integer (context, top, "sunset", mktime (&yesterday));

       echttp_json_add_integer (context, top, "sunrise", sunrise);
    }

    return housealmanac_export (context);
}

static const char *housealmanac_today (const char *method, const char *uri,
                                       const char *data, int length) {
    ParserToken token[1024];
    char pool[65537];

    if (! AlmanacIsConfigured) {
        echttp_error (500, "Service initializing");
        return "";
    }

    time_t now = time(0);

    ParserContext context = echttp_json_start (token, 1024, pool, 65537);

    int top = housealmanac_header (now, context);

    struct tm today = *localtime (&now);
    housealmanac_estimate (AlmanacDb.sunrises, &today);
    echttp_json_add_integer (context, top, "sunrise", mktime (&today));
    housealmanac_estimate (AlmanacDb.sunsets, &today);
    echttp_json_add_integer (context, top, "sunset", mktime (&today));

    return housealmanac_export (context);
}

static void housealmanac_background (int fd, int mode) {

    static time_t LastRenewal = 0;
    time_t now = time(0);

    if (UseHousePortal) {
        static const char *path[] = {"almanac:/almanac"};
        if (now >= LastRenewal + 60) {
            if (LastRenewal > 0)
                houseportal_renew();
            else
                houseportal_register (echttp_port(4), path, 1);
            LastRenewal = now;
        }
    }
    housediscover (now);
    houselog_background (now);
    housedepositor_periodic (now);
}

static void housealmanac_protect (const char *method, const char *uri) {
    echttp_cors_protect(method, uri);
}

static void housealmanac_config_listener (const char *name, time_t timestamp,
                                          const char *data, int length) {

    houselog_event ("SYSTEM", "CONFIG", "LOAD", "FROM DEPOT %s", name);
    if (!houseconfig_update (data)) {
        const char *error = housealmanac_refresh ();
        if (error) {
            DEBUG ("cannot load config: %s\n", error);
        }
    }
}

int main (int argc, const char **argv) {

    // These strange statements are to make sure that fds 0 to 2 are
    // reserved, since this application might output some errors.
    // 3 descriptors are wasted if 0, 1 and 2 are already open. No big deal.
    //
    open ("/dev/null", O_RDONLY);
    dup(open ("/dev/null", O_WRONLY));

    echttp_default ("-http-service=dynamic");

    argc = echttp_open (argc, argv);
    if (echttp_dynamic_port()) {
        houseportal_initialize (argc, argv);
        UseHousePortal = 1;
    }
    housediscover_initialize (argc, argv);
    houselog_initialize ("almanac", argc, argv);
    housedepositor_initialize (argc, argv);

    houseconfig_default ("--config=almanac");
    const char *error = houseconfig_load (argc, argv);
    if (error)
        houselog_trace (HOUSE_FAILURE, "CONFIG", "Cannot load: %s\n", error);
    housedepositor_subscribe
        ("config", houseconfig_name(), housealmanac_config_listener);

    echttp_cors_allow_method("GET");
    echttp_protect (0, housealmanac_protect);

    echttp_route_uri ("/almanac/tonight", housealmanac_tonight);
    echttp_route_uri ("/almanac/today", housealmanac_today);
    echttp_route_uri ("/almanac/selftest", housealmanac_selftest);

    echttp_static_route ("/", "/usr/local/share/house/public");
    echttp_background (&housealmanac_background);
    houselog_event ("SERVICE", "almanac", "STARTED", "ON %s", houselog_host());
    echttp_loop();
}


/* HouseAlmanac - A service that calculates Almanac data
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
 * DESIGN
 *
 * This service implements the almanac web API and calculate its data
 * using an algorithm derived from the official United States Naval
 * Observatory (USNO) solar almanac algorithm.
 *
 * This requires the location for which the times should be calculated.
 * Since this location can be provided as a latitude/longitude pair,
 * this program interrogates the clock services until it gets a GPS location.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include "echttp.h"
#include "echttp_cors.h"
#include "echttp_json.h"
#include "echttp_static.h"
#include "houseportalclient.h"

#include "housediscover.h"
#include "houselog.h"
#include "housealmanac_location.h"
#include "housealmanac_cache.h"

#define DEBUG if (echttp_isdebug()) printf

static int housealmanac_head (ParserContext *context, time_t now) {

    static char host[256];
    static char pool[65537];
    static ParserToken token[1024];

    if (host[0] == 0) gethostname (host, sizeof(host));

    ParserContext local = echttp_json_start (token, 1024, pool, sizeof(pool));

    int root = echttp_json_add_object (local, 0, 0);
    echttp_json_add_string (local, root, "host", host);
    echttp_json_add_string (local, root, "proxy", houseportal_server());
    echttp_json_add_integer (local, root, "timestamp", now);

    // Extra information that can be used as status.
    //
    if (housealmanac_location_ready()) {
        int loc = echttp_json_add_object (local, root, "location");
        echttp_json_add_real (local, loc, "lat", housealmanac_location_lat());
        echttp_json_add_real (local, loc, "long", housealmanac_location_long());
    }
    *context = local;
    return root;
}

static int housealmanac_top (ParserContext context, int root) {

    const char *origin = housealmanac_cache_origin();
    time_t updated = housealmanac_cache_updated();

    int top = echttp_json_add_object (context, root, "almanac");
    echttp_json_add_integer (context, top, "priority", 1);
    echttp_json_add_integer (context, top, "updated", updated);
    echttp_json_add_string  (context, top, "origin", origin);
    return top;
}

static void housealmanac_subcontent (ParserContext context,
                                     int top, const char *id,
                                     time_t sunrise, time_t sunset) {

    int sub = echttp_json_add_object (context, top, id);
    echttp_json_add_integer (context, sub, "sunrise", sunrise);
    echttp_json_add_integer (context, sub, "sunset", sunset);
}

static const char *housealmanac_tail (ParserContext context) {

    static char buffer[65537];
    const char *error = echttp_json_export (context, buffer, sizeof(buffer));
    if (error) {
        echttp_error (500, error);
        return "";
    }
    echttp_content_type_json ();
    return buffer;
}

static const char *housealmanac_content (time_t now,
                                         time_t sunrise, time_t sunset) {

    ParserContext context;
    int root = housealmanac_head (&context, now);
    int top = housealmanac_top (context, root);
    echttp_json_add_integer (context, top, "sunset", sunset);
    echttp_json_add_integer (context, top, "sunrise", sunrise);
    return housealmanac_tail (context);
}

static time_t housealmanac_refresh (void) {

    time_t now = time(0);
    const char *error = housealmanac_cache_refresh (now);
    if (error) {
        echttp_error (500, error);
        return 0;
    }
    return now;
}

static const char *housealmanac_tonight (const char *method, const char *uri,
                                         const char *data, int length) {

    time_t now = housealmanac_refresh();
    if (!now) return "";

    time_t sunset;
    time_t sunrise;
    housealmanac_cache_tonight (now, &sunset, &sunrise);
    return housealmanac_content (now, sunrise, sunset);
}

static const char *housealmanac_yesterday (const char *method, const char *uri,
                                           const char *data, int length) {

    time_t now = housealmanac_refresh();
    if (!now) return "";

    time_t sunset;
    time_t sunrise;
    housealmanac_cache_yesterday (&sunrise, &sunset);
    return housealmanac_content (now, sunrise, sunset);
}

static const char *housealmanac_today (const char *method, const char *uri,
                                       const char *data, int length) {

    time_t now = housealmanac_refresh();
    if (!now) return "";

    time_t sunset;
    time_t sunrise;
    housealmanac_cache_today (&sunrise, &sunset);
    return housealmanac_content (now, sunrise, sunset);
}

static const char *housealmanac_tomorrow (const char *method, const char *uri,
                                          const char *data, int length) {

    time_t now = housealmanac_refresh();
    if (!now) return "";

    time_t sunset;
    time_t sunrise;
    housealmanac_cache_tomorrow (&sunrise, &sunset);
    return housealmanac_content (now, sunrise, sunset);
}

static const char *housealmanac_status (const char *method, const char *uri,
                                       const char *data, int length) {

    time_t sunset;
    time_t sunrise;
    time_t now = housealmanac_refresh();
    if (!now) return "";

    ParserContext context;
    int root = housealmanac_head (&context, now);
    int top = housealmanac_top (context, root);

    housealmanac_cache_yesterday (&sunrise, &sunset);
    housealmanac_subcontent (context, top, "yesterday", sunrise, sunset);

    housealmanac_cache_today (&sunrise, &sunset);
    housealmanac_subcontent (context, top, "today", sunrise, sunset);

    housealmanac_cache_tomorrow (&sunrise, &sunset);
    housealmanac_subcontent (context, top, "tomorrow", sunrise, sunset);

    housealmanac_cache_tonight (now, &sunset, &sunrise);
    housealmanac_subcontent (context, top, "tonight", sunrise, sunset);

    return housealmanac_tail (context);
}

static void housealmanac_background (int fd, int mode) {

    static time_t LastCall = 0;
    time_t now = time(0);

    if (now == LastCall) return;
    LastCall = now;

    houseportal_background (now);
    housediscover (now);
    houselog_background (now);
    housealmanac_location_background (now);

    if (housealmanac_location_ready()) {
        // GPS coordinates are needed for the almanac data.
        housealmanac_cache_location (housealmanac_location_lat(),
                                     housealmanac_location_long());
        housealmanac_cache_background (now);
    }
}

static void housealmanac_protect (const char *method, const char *uri) {
    echttp_cors_protect(method, uri);
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
        static const char *path[] = {"almanac:/almanac"};
        houseportal_initialize (argc, argv);
        houseportal_declare (echttp_port(4), path, 1);
    }

    housediscover_initialize (argc, argv);
    houselog_initialize ("almanac", argc, argv);
    housealmanac_location_initialize (argc, argv);

    echttp_cors_allow_method("GET");
    echttp_protect (0, housealmanac_protect);

    echttp_route_uri ("/almanac/tonight",   housealmanac_tonight);
    echttp_route_uri ("/almanac/today",     housealmanac_today);
    echttp_route_uri ("/almanac/yesterday", housealmanac_yesterday);
    echttp_route_uri ("/almanac/tomorrow",  housealmanac_tomorrow);
    echttp_route_uri ("/almanac/status",    housealmanac_status);

    echttp_static_route ("/", "/usr/local/share/house/public");
    echttp_background (&housealmanac_background);
    houselog_event ("SERVICE", "almanac", "STARTED", "ON %s", houselog_host());
    echttp_loop();
}


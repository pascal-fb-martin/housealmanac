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
#include "echttp_libc.h"
#include "echttp_cors.h"
#include "echttp_json.h"
#include "echttp_static.h"
#include "houseportalclient.h"

#include "housediscover.h"
#include "houselog.h"
#include "housealmanac_location.h"
#include "housealmanac_calculate.h"

#define DEBUG if (echttp_isdebug()) printf

static const char *housealmanac_content (time_t sunset, time_t sunrise,
                                         time_t timestamp) {

    static char host[256];
    static char buffer[65537];
    static char pool[65537];

    if (host[0] == 0) gethostname (host, sizeof(host));

    ParserToken token[1024];
    ParserContext context = echttp_json_start (token, 1024, pool, sizeof(pool));

    const char *origin = housealmanac_calculate_origin();

    int root = echttp_json_add_object (context, 0, 0);
    echttp_json_add_string (context, root, "host", host);
    echttp_json_add_string (context, root, "proxy", houseportal_server());
    echttp_json_add_integer (context, root, "timestamp", (long)time(0));

    // Extra information that can be used as status.
    //
    int loc = echttp_json_add_object (context, root, "location");
    if (housealmanac_location_ready()) {
        echttp_json_add_real (context, loc, "lat", housealmanac_location_lat());
        echttp_json_add_real (context, loc, "long", housealmanac_location_long());
    }

    int top = echttp_json_add_object (context, root, "almanac");
    echttp_json_add_integer (context, top, "priority", 1);
    echttp_json_add_integer (context, top, "updated", timestamp);
    echttp_json_add_string (context, top, "origin", origin);
    echttp_json_add_integer (context, top, "sunset", sunset);
    echttp_json_add_integer (context, top, "sunrise", sunrise);

    const char *error = echttp_json_export (context, buffer, sizeof(buffer));
    if (error) {
        echttp_error (500, error);
        return "";
    }
    echttp_content_type_json ();
    return buffer;
}

static const char *housealmanac_tonight (const char *method, const char *uri,
                                         const char *data, int length) {

    time_t now = time(0);
    time_t sunset;
    time_t sunrise;

    const char *error = housealmanac_calculate_tonight (now, &sunset, &sunrise);
    if (error) {
        echttp_error (500, error);
        return "";
    }
    return housealmanac_content (sunset, sunrise, now);
}

static const char *housealmanac_today (const char *method, const char *uri,
                                       const char *data, int length) {

    time_t now = time(0);
    time_t sunset;
    time_t sunrise;

    const char *error = housealmanac_calculate_today (now, &sunrise, &sunset);
    if (error) {
        echttp_error (500, error);
        return "";
    }
    return housealmanac_content (sunset, sunrise, now);
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
        housealmanac_calculate_location (housealmanac_location_lat(),
                                         housealmanac_location_long());
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

    echttp_cors_allow_method("GET");
    echttp_protect (0, housealmanac_protect);

    echttp_route_uri ("/almanac/status", housealmanac_today);
    echttp_route_uri ("/almanac/tonight", housealmanac_tonight);
    echttp_route_uri ("/almanac/today", housealmanac_today);

    echttp_static_route ("/", "/usr/local/share/house/public");
    echttp_background (&housealmanac_background);
    houselog_event ("SERVICE", "almanac", "STARTED", "ON %s", houselog_host());
    echttp_loop();
}


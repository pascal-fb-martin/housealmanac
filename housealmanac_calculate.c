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
 * housealmanac_local.c - Calculate sunset and sunrise time locally
 *
 * SYNOPSYS:
 *
 * This code was generated using Google Gemini, then modified to fit
 * within the HouseAlmanac project.
 *
 * SYNOPSYS:
 *
 * void housealmanac_calculate_location (double latitude, double longitude);
 *
 *    Set the current location for which future almanac data is needed.
 *    The timezone parameter is the offset with UTC in seconds.
 *    No almanac data will be available until the location is known.
 *
 * const char *housealmanac_calculate_today (time_t now,
 *                                           time_t *rise, time_t *set);
 *
 *    Get the sunrise and sunset times for today. Return null on success,
 *    an error message otherwise.
 *
 * const char *housealmanac_calculate_tonight (time_t now,
 *                                             time_t *set, time_t *rise);
 *
 *    Get the sunset and sunrise times for the upcoming night. Return null
 *    on success, an error message otherwise.
 *
 * const char *housealmanac_calculate_origin (void);
 *
 *    Return a static string describing the origin of the data.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "housealmanac_calculate.h"

static int AlmanacHasLocation = 0;
static double AlmanacLatitude = 0.0;
static double AlmanacLongitude = 0.0;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Helper function to convert degrees to radians
static double deg2rad(double deg) {
    return (deg * M_PI / 180.0);
}

// Helper function to convert radians to degrees
static double rad2deg(double rad) {
    return (rad * 180.0 / M_PI);
}

// Helper function to clamp values between 0 and 360 degrees
static double clamp_360(double degrees) {
    while (degrees < 0.0) degrees += 360.0;
    while (degrees >= 360.0) degrees -= 360.0;
    return degrees;
}

// Core calculation function
// returns 1 if successful, 0 if the sun never rises/sets (Polar day/night)
static int calculate_sun_time(int yearday,
                              double latitude, double longitude,
                              double timezone, int is_sunrise,
                              int *out_hours, int *out_minutes) {

    double N = yearday; // Minimize changes below.

    // 2. Convert longitude to hour value and calculate an approximate time
    double lngHour = longitude / 15.0;
    double t = is_sunrise ? (N + ((6.0 - lngHour) / 24.0)) : (N + ((18.0 - lngHour) / 24.0));

    // 3. Calculate the Sun's mean anomaly
    double M = (0.9856 * t) - 3.289;

    // 4. Calculate the Sun's true longitude
    double L = M + (1.916 * sin(deg2rad(M))) + (0.020 * sin(deg2rad(2.0 * M))) + 282.634;
    L = clamp_360(L);

    // 5. Calculate the Sun's right ascension (RA)
    double RA = rad2deg(atan(0.91746 * tan(deg2rad(L))));
    RA = clamp_360(RA);

    // Right ascension value needs to be in the same quadrant as L
    double Lquadrant  = floor(L / 90.0) * 90.0;
    double RAquadrant = floor(RA / 90.0) * 90.0;
    RA = RA + (Lquadrant - RAquadrant);

    // Convert right ascension to hours
    RA = RA / 15.0;

    // 6. Calculate the Sun's declination
    double sinDec = 0.39782 * sin(deg2rad(L));
    double cosDec = cos(asin(sinDec));

    // 7. Calculate the Sun's local hour angle
    // -0.833 degrees accounts for refraction and the sun's radius at the horizon
    double cosH = (sin(deg2rad(-0.833)) - (sinDec * sin(deg2rad(latitude)))) / (cosDec * cos(deg2rad(latitude)));

    if (cosH > 1.0) return 0;  // Sun never rises (Polar Night)
    if (cosH < -1.0) return 0; // Sun never sets (Polar Day)

    // 8. Calculate local hour angle (H) and convert to hours
    double H = is_sunrise ? (360.0 - rad2deg(acos(cosH))) : rad2deg(acos(cosH));
    H = H / 15.0;

    // 9. Calculate local mean time of rising/setting
    double T = H + RA - (0.06571 * t) - 6.622;

    // 10. Adjust back to UTC, then apply local timezone offset
    double UT = T - lngHour;
    double localT = UT + timezone;
    
    // Normalize time to a 24-hour range
    while (localT < 0.0) localT += 24.0;
    while (localT >= 24.0) localT -= 24.0;

    // Convert decimal hours into integer hours and minutes
    *out_hours = (int)floor(localT);
    *out_minutes = (int)round((localT - *out_hours) * 60.0);
    
    if (*out_minutes == 60) {
        *out_minutes = 0;
        *out_hours = (*out_hours + 1) % 24;
    }

    return 1;
}

void housealmanac_calculate_location (double latitude, double longitude) {

    AlmanacLatitude = latitude;
    AlmanacLongitude = longitude;
    AlmanacHasLocation = 1;
}

const char *housealmanac_calculate_today (time_t now,
                                          time_t *rise, time_t *set) {

    if (!AlmanacHasLocation) return "unknown location";

    int rise_h, rise_m, set_h, set_m;
    struct tm today = *localtime (&now);
    double timezone = (double)(today.tm_gmtoff) / 3600.0;

    if (!calculate_sun_time(today.tm_yday, AlmanacLatitude, AlmanacLongitude,
                            timezone, 1, &rise_h, &rise_m)) {
        return "No sunrise detected (Polar day/night)";
    }

    if (!calculate_sun_time(today.tm_yday, AlmanacLatitude, AlmanacLongitude,
                            timezone, 0, &set_h, &set_m)) {
        return "No sunset detected (Polar day/night)";
    }

    today.tm_sec = 0;
    today.tm_min = rise_m;
    today.tm_hour = rise_h;
    *rise = mktime (&today);

    today.tm_sec = 0;
    today.tm_min = set_m;
    today.tm_hour = set_h;
    *set = mktime (&today);

    return 0;
}

const char *housealmanac_calculate_tonight (time_t now,
                                            time_t *set, time_t *rise) {

    if (!AlmanacHasLocation) return "unknown location";

    int rise_h, rise_m, set_h, set_m;
    struct tm today = *localtime (&now);
    double timezone = (double)(today.tm_gmtoff) / 3600.0;

    if (!calculate_sun_time(today.tm_yday, AlmanacLatitude, AlmanacLongitude,
                            timezone, 1, &rise_h, &rise_m)) {
        return "No sunrise detected (Polar day/night)";
    }

    today.tm_sec = 0;
    today.tm_min = rise_m;
    today.tm_hour = rise_h;
    *rise = mktime (&today);

    if (now <= *rise) {
        // That night is not over, look for yesterday's sunset
        now -= (24*60*60);
        struct tm yesterday = *localtime (&now);

        if (!calculate_sun_time(yesterday.tm_yday,
                                AlmanacLatitude, AlmanacLongitude,
                                timezone, 0, &set_h, &set_m)) {
            return "No sunset detected (Polar day/night)";
        }

        yesterday.tm_sec = 0;
        yesterday.tm_min = set_m;
        yesterday.tm_hour = set_h;
        *set = mktime (&yesterday);

    } else {
        // That night is over, look for today's sunset.
        if (!calculate_sun_time(today.tm_yday,
                                AlmanacLatitude, AlmanacLongitude,
                                timezone, 0, &set_h, &set_m)) {
            return "No sunset detected (Polar day/night)";
        }

        today.tm_sec = 0;
        today.tm_min = set_m;
        today.tm_hour = set_h;
        *set = mktime (&today);

        // We must recalculate sunrise: this is tomorrow's sunrise.
        now += (24*60*60);
        struct tm tomorrow = *localtime (&now);

        if (!calculate_sun_time(tomorrow.tm_yday,
                                AlmanacLatitude, AlmanacLongitude,
                                timezone, 1, &rise_h, &rise_m)) {
            return "No sunrise detected (Polar day/night)";
        }

        tomorrow.tm_sec = 0;
        tomorrow.tm_min = rise_m;
        tomorrow.tm_hour = rise_h;
        *rise = mktime (&tomorrow);
    }

    return 0;
}

const char *housealmanac_calculate_origin (void) {
    return "calculated";
}


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
 * housealmanac_calculate.c - Calculate sunset and sunrise time locally
 *
 * SYNOPSYS:
 *
 * This code was generated using Google Gemini, then modified to fit
 * within the HouseAlmanac project. That code claims to be based on
 * the official United States Naval Observatory method.
 *
 * SYNOPSYS:
 *
 * const char *housealmanac_calculate (const struct tm *date,
 *                                     double latitude, double longitude,
 *                                     time_t *rise, time_t *set);
 *
 *    Calculate both sunrise and sunset for the specified date.
 *    Return null on success, an error message otherwise.
 */

#include <math.h>
#include <time.h>

#include "housealmanac_calculate.h"

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

const char *housealmanac_calculate (const struct tm *date,
                                    double latitude, double longitude,
                                    time_t *rise, time_t *set) {

    int hour, minute;
    double timezone = (double)(date->tm_gmtoff) / 3600.0;

    if (!calculate_sun_time(date->tm_yday, latitude, longitude,
                            timezone, 1, &hour, &minute)) {
        return "No sunrise detected (Polar day/night)";
    }
    struct tm sun = *date;
    sun.tm_sec = 0;
    sun.tm_min = minute;
    sun.tm_hour = hour;
    *rise = mktime (&sun);

    if (!calculate_sun_time(date->tm_yday, latitude, longitude,
                            timezone, 0, &hour, &minute)) {
        return "No sunset detected (Polar day/night)";
    }
    sun.tm_min = minute;
    sun.tm_hour = hour;
    *set = mktime (&sun);

    return 0;
}


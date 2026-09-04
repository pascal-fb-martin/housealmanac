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
 * housealmanac_cache.h -- Reuse calculated sunrise and sunset times.
 */

void housealmanac_cache_location (double latitude, double longitude);

const char *housealmanac_cache_refresh (time_t now);

void housealmanac_cache_yesterday (time_t *rise, time_t *set);
void housealmanac_cache_today (time_t *rise, time_t *set);
void housealmanac_cache_tomorrow (time_t *rise, time_t *set);

void housealmanac_cache_tonight (time_t now, time_t *set, time_t *rise);

time_t housealmanac_cache_updated (void);
const char *housealmanac_cache_origin (void);
void housealmanac_cache_background (time_t now);

/* HouseAlmanac - A service that calculates almanac data
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
 */

void housealmanac_calculate_location (double latitude, double longitude);

const char *housealmanac_calculate_today (time_t now,
                                          time_t *rise, time_t *set);
const char *housealmanac_calculate_tonight (time_t now,
                                            time_t *set, time_t *rise);

const char *housealmanac_calculate_origin (void);


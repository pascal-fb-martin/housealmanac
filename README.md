# HouseAlmanac
A fallback service providing rough estimates for almanac information

## Overview

This service provides very rough data about the current day:
- Sunrise time.
- Sunset time.

These values are estimated based on mid-month static data (from a configuration file). The estimation uses a linear regression method.

The purpose of this service is either as a test tools for applications that depend on an almanac service, or as a fallback when Internet connectivity is not available.

This service has no web UI.

## Installation

This service depends on the House series environment:
* Install git, icoutils, openssl (libssl-dev).
* Install [echttp](https://github.com/pascal-fb-martin/echttp)
* Install [houseportal](https://github.com/pascal-fb-martin/houseportal)
* Clone this repository.
* make rebuild
* sudo make install

## Configuration

The HouseAlmanac service loads its configuration from the almanac.json file, which syntax matches the example below:
```
{
    "almanac": {
        "sunrise": ["6:58","6:33","6:41","6:24","5:46","5:42","5:50","6:16","6:37","6:54","6:28","6:49"],
        "sunset": ["17:33","17:59","19:19","19:49","20:14","20:31","20:40","19:57","19:15","19:33","17:04","17:08"],
        "dst": ["03/09","11/02"]
    }
}
```

All times are local time. the `dst` array represents the two DST changes dates, spring and fall.

An example of configuration for the Los Angeles area is provided.

## Web API

The HouseAlmanac service support the following web requests:
```
GET /almanac/nextnight
```
Return JSON data that contains the following data for the next night:
- .host: name of the responding host.
- .timestamp: time of the response, in seconds.
- .almanac.priority: a priority level that matches the data quality.
- .almanac.sunset: local time of sunset this day (string: HH:MM).
- .almanac.sunrise: local time of sunrise next day(string: HH:MM).

For this statically configured fallback service, the priority is always 1 (low).

```
GET /almanac/selftest
```
Return the complete sunset and sunrise information for the whole year. In this response, both the sunset and sunrise fields are arrays of 365 entries. Leap years are not considered (this is just a test endpoint).

This endpoint forces HouseAlmanac to prints extensive traces to standard output. Do not use in production.

## Further References

More precise sunset and sunrise information can be obtained from [SunriseSunset](https://sunrise-sunset.org). See their [API](https://sunrise-sunset.org/api) for more information.


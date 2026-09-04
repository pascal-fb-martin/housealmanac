# HouseAlmanac
A service providing almanac information calculated locally

## Overview

This service provides almanac data about the current, previous and next day:

- Sunrise time.
- Sunset time.

These values are calculated based on the United States Naval Observatory method.

The purpose of this service is either as a test tools for applications that depend on an almanac service, or as a fallback when Internet connectivity is not available.

## Installation

This service depends on the House series environment:

* Install git, icoutils, openssl (libssl-dev).
* Install [echttp](https://github.com/pascal-fb-martin/echttp)
* Install [houseportal](https://github.com/pascal-fb-martin/houseportal)
* Clone this repository.
* make rebuild
* sudo make install

In addition one instance of the [houseclock](https://github.com/pascal-fb-martin/houseclock) service must be running on the local network, with a GPS receiver attached (to provide the latitude and longitude). This does not need to run (or be installed) on the same machine as HouseAlmanac.

## Configuration

The HouseAlmanac service does not need configuration files.

If no [houseclock](https://github.com/pascal-fb-martin/houseclock) service instance is present, the `-latitude` and `-longitude` command line option must be used as a substitute.

## Web API

The HouseAlmanac service support the following web requests:

```
GET /almanac/tonight
```

Return JSON data that contains the following data for the upcoming or current
night:

- .host: name of the responding host.
- .timestamp: system time of the response, in seconds.
- .almanac.priority: a priority level that matches the data quality.
- .almanac.sunset: system time of tonight's sunset, in seconds (integer).
- .almanac.sunrise: system time of tonight's sunrise, in seconds (integer).

The sunset time can be in the past, typically at night time.

For this statically configured fallback service, the priority is always 1 (low).

```
GET /almanac/yesterday
GET /almanac/today
GET /almanac/tomorrow
```

Return JSON data that contains the almanac data for the specified day. The
format is the same as for the `/almanac/tonight` endpoint, except that both
the sunrise and sunset values are always for the specified day.

```
GET /almanac/status
```

Return the complete sunset and sunrise information: current, past and next day as well as the `tonight` almanac. Each set is provided as a separate object:

- `.almanac.today`: sunrise and sunset time for the current day.
- `.almanac.yesterday`: sunrise and sunset time for the previous day.
- `.almanac.tomorrow`: sunrise and sunset time for the next day.
- `.almanac.tonight`: sunrise and sunset time for the upcoming or current night.

## Further References

More precise sunset and sunrise information can be obtained from [SunriseSunset](https://sunrise-sunset.org). See their [API](https://sunrise-sunset.org/api) for more information.

## Debian Packaging

The provided Makefile supports building private Debian packages. These are _not_ official packages:

- They do not follow all Debian policies.

- They are not built using Debian standard conventions and tools.

- The packaging is not separate from the upstream sources, and there is
  no source package.

To build a Debian package, use the `debian-package` target:

```
make debian-package
```


// TECHRAD JS

var config={}; // CONFIG_SECONDS, CONFIG_HOURVIBES, CONFIG_FAHRENHEIT, CONFIG_24H, CONFIG_SETCITY, CONFIG_CITYID
var locationOptions = { "timeout": 72000, "maximumAge": 2000000 };
var prevcity = ""; // previous city if set
//var appid = "869c6da7ab3f807c4b15fd7574786e72"; // Openweathermap API ID
var appid = "d204cb99d4331fffa26340b8e03bbe17"; // Openweathermap API ID
var newForecastLoaded = false;

//======================================
// GET ICON FROM WEATHER ID
//======================================
function iconFromWeatherId(weatherId) {
  if (weatherId < 600) {
    return 2;
  } else if (weatherId < 700) {
    return 3;
  } else if (weatherId > 800) {
    return 1;
  } else {
    return 0;
  }
}

//======================================
// TIME CONVERTER - converts UTC to ampm or 24 hour time
// depends on 24h time config setting
//======================================
function timeConverter(timeUTC) {
	var ampm = "";
	var temp_date = new Date(timeUTC * 1000);
	var temp_hours = temp_date.getHours();	
	var temp_minutes = "0" + temp_date.getMinutes();

	if (config.CONFIG_24H == 0) {
		if (temp_hours == 0) {
			temp_hours = 12;
			ampm = "AM";
		}
		if ((temp_hours > 0) && (temp_hours < 12)) {
			ampm = "AM";
		}
		if (temp_hours == 12) {
			ampm = "PM";
		}
		if (temp_hours > 12) {
			temp_hours = temp_hours - 12;
			ampm = "PM";
		}
	}
	
	if (config.CONFIG_24H == 0) {
		return temp_hours + ':' + temp_minutes.substr(temp_minutes.length-2) + " " + ampm;		
	}
	else {
		return temp_hours + ':' + temp_minutes.substr(temp_minutes.length-2);				
	}
}

//======================================
// TEMPERATURE CONVERTER - Kelvin to F, Kelvin to C
// depends on Fahrenheit config setting
//======================================
function tempConverter(intemp) {
    if (intemp == "-") {
        return intemp;
    }
    else {
        if (config.CONFIG_FAHRENHEIT == 1) {
            return Math.round(((intemp - 272.15) * 9/5) + 32);
        }
        else {
            return Math.round(intemp - 272.15);			
        }
    }
}

//======================================
// WINDSPEED CONVERTER - meters per second to km/h or mph
// depends on Fahrenheit/metric config setting
//======================================
function windConverter(inwind) {
	if (config.CONFIG_FAHRENHEIT == 1) {		  
		return Math.round(((inwind * 60 * 60)/1000)*0.6214) + " mph"; // return mph
	}
	else {
		return Math.round((inwind * 60 * 60)/1000) + " km/h"; // return km/h	
	}
}

//======================================
// FETCH CURRENT WEATHER DATA
//======================================
function fetchWeather(favcity, latitude, longitude) {
//    newForecastLoaded = false;
    
	// save location data for cache
	if (latitude != null) {
		localStorage.setItem("latitude", latitude);
	}
	if (longitude != null) {
		localStorage.setItem("longitude", longitude);
	}

    // load weather data based on city name, city id, gps
	var req = new XMLHttpRequest();
	if (favcity != null) {
        if (config.CONFIG_CITYID == 0) {
            console.log("fetching city name");
            req.open('GET', "http://api.openweathermap.org/data/2.5/weather?" +
                     "q=" + favcity + "&cnt=1&APPID=" + appid, true);
        }
        else {
            console.log("fetching city id");
            req.open('GET', "http://api.openweathermap.org/data/2.5/weather?" +
                     "id=" + favcity + "&cnt=1&APPID=" + appid, true);
        }
    }
    else {
        console.log("fetching gps");
		req.open('GET', "http://api.openweathermap.org/data/2.5/weather?" +
                 "lat=" + latitude + "&lon=" + longitude + "&cnt=1&APPID=" + appid, true);
	}
//	req.onload = function (e) {
	req.onreadystatechange = function (e) {
		if (req.readyState == 4) {
			if (req.status == 200) {
//				console.log(req.responseText);
				var response = JSON.parse(req.responseText);
                if (response.cod == "404") {
                    //console.log("404 no data found");
                    localStorage.setItem("temperature", "-");
                    localStorage.setItem("city", "no data");
                    localStorage.setItem("icon", 4);
                }
                else {
                    var temperature = response.main.temp; // in K
                    var windspeed = response.wind.speed; // in mps
                    var icon = iconFromWeatherId(response.weather[0].id);
                    var city = response.name;
                    var sunrise_UTC = response.sys.sunrise;
                    var sunset_UTC = response.sys.sunset;
                    var fetch_timestamp = new Date(response.dt * 1000);

                    // Save weather data for cache
                    localStorage.setItem("icon", icon);
                    localStorage.setItem("temperature", temperature);
                    localStorage.setItem("windspeed", windspeed);
                    localStorage.setItem("city", city);
                    localStorage.setItem("sunrise_time", sunrise_UTC);
                    localStorage.setItem("sunset_time", sunset_UTC);
                    localStorage.setItem("weather_timestamp", fetch_timestamp);
                }
			}
		}
	}
	req.send(null);

    // load forecast data based on city name, city id, gps
	var reqf = new XMLHttpRequest();
	if (favcity != null) {
        if (config.CONFIG_CITYID == 0) {
            //console.log("Forecast loading favcity");
            reqf.open('GET', "http://api.openweathermap.org/data/2.5/forecast?" +
                      "q=" + favcity + "&cnt=4&APPID=" + appid, true);
        }
        else {
            reqf.open('GET', "http://api.openweathermap.org/data/2.5/forecast?" +
                     "id=" + favcity + "&cnt=4&APPID=" + appid, true);
        }
    }
	else {
        //console.log("Forecast loading GPS");
		reqf.open('GET', "http://api.openweathermap.org/data/2.5/forecast?" +
                  "lat=" + latitude + "&lon=" + longitude + "&cnt=4&APPID=" + appid, true);
	}
//	reqf.onload = function (x) {
	reqf.onreadystatechange = function (x) {
		if (reqf.readyState == 4) {
			if (reqf.status == 200) {
//				console.log(reqf.responseText);
				var responsef = JSON.parse(reqf.responseText);
                if (responsef.cod == "404") {
                    //console.log("Error 404 forecast");
                    localStorage.setItem("forecast_icon", 4);
                }
                else {
                    var min_temp = Math.min(responsef.list[0].main.temp_min, responsef.list[1].main.temp_min, responsef.list[2].main.temp_min, responsef.list[3].main.temp_min); // in K
                    var max_temp = Math.max(responsef.list[0].main.temp_max, responsef.list[1].main.temp_max, responsef.list[2].main.temp_max, responsef.list[3].main.temp_max); // in K
                    var forecast_icon = iconFromWeatherId(responsef.list[0].weather[0].id);

                    // Save forecast data for cache
                    localStorage.setItem("min_temp", min_temp);
                    localStorage.setItem("max_temp", max_temp);
                    localStorage.setItem("forecast_icon", forecast_icon);
                }
                readCachedData();
			}
		}
	}
	reqf.send(null);
}


//======================================
// READ CACHED DATA FROM PHONE
//======================================
function readCachedData() {
	if (localStorage.getItem("weather_timestamp") != null) {
		var cached_icon = Math.floor(localStorage.getItem("icon"));
		var cached_temperature = localStorage.getItem("temperature");
		var cached_windspeed = localStorage.getItem("windspeed");
		var cached_city = localStorage.getItem("city");
		var cached_sunrise_UTC = localStorage.getItem("sunrise_time");
		var cached_sunset_UTC = localStorage.getItem("sunset_time");
		var cached_timestamp_time = localStorage.getItem("timestamp_time");
		var cached_forecast_icon = Math.floor(localStorage.getItem("forecast_icon"));
		var cached_min_temp = localStorage.getItem("min_temp");
		var cached_max_temp = localStorage.getItem("max_temp");

		// do temperature conversions
		var temperature_formatted = tempConverter(cached_temperature);
		var min_temp_formatted = tempConverter(cached_min_temp);
		var max_temp_formatted = tempConverter(cached_max_temp);
		
		// do windspeed conversions
		var windspeed_formatted = windConverter(cached_windspeed);
		
		// do time conversions from UTC to ampm/24 hour time
		var cached_sunrise_time = timeConverter(cached_sunrise_UTC);
		var cached_sunset_time = timeConverter(cached_sunset_UTC);
		
		Pebble.sendAppMessage({
	      	"WEATHER_ICON":cached_icon,
	      	"WEATHER_TEMPERATURE":temperature_formatted + "\u00B0",
	      	"WEATHER_CITY":cached_city,
		  	"WEATHER_SUNTIMES":cached_sunrise_time + "\n" + cached_sunset_time,
		  	"WEATHER_FORECASTICON":cached_forecast_icon,
          	"WEATHER_MINMAXTEMP": min_temp_formatted + "-" + max_temp_formatted + "\u00B0",
			"WEATHER_MISC":windspeed_formatted,
		});
	}
	else { 
		//console.log("Error, no data in cache");
		Pebble.sendAppMessage({
          "WEATHER_ICON":4,
          "WEATHER_SUNTIMES":"",
          "WEATHER_FORECASTICON":4,
          "WEATHER_MINMAXTEMP":"",
          "WEATHER_TEMPERATURE":"",
          "WEATHER_CITY":"no data"
  		});
	}
}

//======================================
// LOCATION SUCCESS - fetch weather and forecast data
//======================================
function locationSuccess(pos) {
  	var coordinates = pos.coords;
  	fetchWeather(null, coordinates.latitude, coordinates.longitude);
//    console.log("Coordinates " + coordinates.latitude + " ," + coordinates.longitude);
}

//======================================
// LOCATION ERROR - fetch weather and forecast data using cached lat/long
//======================================
function locationError(err) {
	//console.log('location error (' + err.code + '): ' + err.message);
	if (localStorage.getItem("latitude") != null) {
        fetchWeather(null, localStorage.getItem("latitude"), localStorage.getItem("longitude"));
	}
	else {
  		Pebble.sendAppMessage({
          "WEATHER_ICON":4,
          "WEATHER_SUNTIMES":"",
          "WEATHER_FORECASTICON":4,
          "WEATHER_MINMAXTEMP":"",
  		  "WEATHER_TEMPERATURE":"GPS",
          "WEATHER_CITY":"no GPS"
  		});
	}
}

//======================================
// DEFAULT CONFIG
//======================================
function defaultConfig() {
	o={};
    o["CONFIG_REVERSE"]=0;		// black background layout
    o["CONFIG_COLORTICKS"]=1;
	o["CONFIG_24H"]=1; 			// use 24 hour time
	o["CONFIG_SECONDS"]=1;      // show second hand
	o["CONFIG_HOURVIBES"]=0;	// don't vibrate on the hour
	o["CONFIG_FAHRENHEIT"]=0;   // use metric units
    o["CONFIG_CITYID"]=0;		// no city ID, use city name or GPS
	o["CONFIG_SETCITY"]="";		// no city name, use GPS
    o["CONFIG_DISTANCE"]=1;      // show distance
    o["CONFIG_BLUETHEME"]=0;      // blue theme
	return o;
}

//======================================
// SEND CONFIG TO WATCH
//======================================
function sendConfig() {
    Pebble.sendAppMessage({
      "CONFIG_REVERSE": config.CONFIG_REVERSE,
      "CONFIG_COLORTICKS": config.CONFIG_COLORTICKS,
      "CONFIG_SECONDS":config.CONFIG_SECONDS,
      "CONFIG_HOURVIBES":config.CONFIG_HOURVIBES,
      "CONFIG_DISTANCE": config.CONFIG_DISTANCE,
      "CONFIG_BLUETHEME": config.CONFIG_BLUETHEME,
      });
}

//======================================
// ON INITIAL LOAD
//======================================
Pebble.addEventListener("ready", function(e) {
	// load config from phone storage
	var e=localStorage.getItem("techradconfig");
	"string" == typeof e && (config=JSON.parse(e));
	if (JSON.stringify(config) == "{}") {
		//load default config
		config = defaultConfig();
	}
//	console.log("config is " + JSON.stringify(config));
    sendConfig();
                        
	// load cached data if less than 58 minutes, otherwise fetch fresh data
	var current_time = new Date();
	var weather_timestamp = new Date(localStorage.getItem("weather_timestamp"));
	var timediff = Math.floor(current_time.getTime()/1000/60) - Math.floor(weather_timestamp.getTime()/1000/60);
	console.log("Time diff is " + timediff);
	if (timediff < 60) {
	  	console.log("Within refresh limit, loading cached data");
		readCachedData();
	}
	else {		
		// load weather using favorite city name, else city ID, else use GPS location
		if ((config.CONFIG_SETCITY != "") && (config.CONFIG_SETCITY != "undefined")) {
			console.log("Loading config setcity " + config.CONFIG_SETCITY);
			fetchWeather(config.CONFIG_SETCITY, null, null);
		}
        else {
            console.log("Loading from GPS position");
            locationWatcher = window.navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions);
        }
        if (newForecastLoaded = true) {
            readCachedData();
        }
	}
});

//======================================
// ON APPMESSAGE RECEIVED - fetch fresh data
//======================================
Pebble.addEventListener("appmessage", function(e) {	
	if ((config.CONFIG_SETCITY != "") || (config.CONFIG_SETCITY != "undefined")) {
		fetchWeather(config.CONFIG_SETCITY, null, null);
	}
	else {
        locationWatcher = window.navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions);
	}

});
 
//======================================
// CONFIGURATION WINDOW - hack to show offline HTML page
// Thanks to Tom Gidden, loads current settings too
//======================================
Pebble.addEventListener("showConfiguration", function(e) {
	prevcity = config.CONFIG_SETCITY; // set previous city before opening config window
	Pebble.openURL("data:text/html,"+encodeURIComponent(
	'<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1"></head><body><header><h1><span>TechRad 2.7</span></h1></header><form onsubmit="return s(this)"><p><input type="checkbox" id="CONFIG_REVERSE" class="showhide"><label for="CONFIG_REVERSE">Reversed layout with white background<br>(default is black background)</label><p><input type="checkbox" id="CONFIG_BLUETHEME" class="showhide"><label for="CONFIG_BLUETHEME">Blue theme for graphics<br>(default is red)</label><p><input type="checkbox" id="CONFIG_24H" class="showhide"><label for="CONFIG_24H">24 hour time<br>(default is 12 hour am/pm time)</label><p><input type="checkbox" id="CONFIG_SECONDS" class="showhide"><label for="CONFIG_SECONDS">Show second hand</label><p><input type="checkbox" id="CONFIG_HOURVIBES" class="showhide"><label for="CONFIG_HOURVIBES">Vibrate at the start of every hour</label><p><input type="checkbox" id="CONFIG_FAHRENHEIT" class="showhide"><label for="CONFIG_FAHRENHEIT">Use Fahrenheit for temperature and mph for windspeed<br>(default is Centigrade and km/h)</label><p><input type="checkbox" id="CONFIG_DISTANCE" class="showhide"><label for="CONFIG_DISTANCE">Show distance walked<br>(default is no. of steps walked)</label><p><input type="checkbox" id="CONFIG_CITYID" class="showhide"><label for="CONFIG_CITYID">Use OpenWeathermap city ID<br>(default is to search for city name)</label><p><input type="text" id="CONFIG_SETCITY" class="showhide"><label for="CONFIG_SETCITY"><br>Set city name or city ID (leave empty to use GPS)</label><p><p><input type="submit" value="Save Settings"></form><p><footer>By Mango Lazi</footer><script>function s(e){o={};o["CONFIG_REVERSE"]=document.getElementById("CONFIG_REVERSE").checked?1:0;o["CONFIG_BLUETHEME"]=document.getElementById("CONFIG_BLUETHEME").checked?1:0;o["CONFIG_24H"]=document.getElementById("CONFIG_24H").checked?1:0;o["CONFIG_SECONDS"]=document.getElementById("CONFIG_SECONDS").checked?1:0;o["CONFIG_HOURVIBES"]=document.getElementById("CONFIG_HOURVIBES").checked?1:0;o["CONFIG_FAHRENHEIT"]=document.getElementById("CONFIG_FAHRENHEIT").checked?1:0;o["CONFIG_DISTANCE"]=document.getElementById("CONFIG_DISTANCE").checked?1:0;o["CONFIG_CITYID"]=document.getElementById("CONFIG_CITYID").checked?1:0;o["CONFIG_SETCITY"]=document.getElementById("CONFIG_SETCITY").value;return window.location.href="pebblejs://close#"+JSON.stringify(o),!1}var d="_CONFDATA_";for(var i in d)d.hasOwnProperty(i)&&(document.getElementById(i).checked=d[i]);document.getElementById("CONFIG_SETCITY").value=d[i];</script></body></html>\n<!--.html'.replace('"_CONFDATA_"',JSON.stringify(config),"g")))});


//======================================
// ON CONFIGURATION WINDOW CLOSED - sync settings with watch
//======================================
Pebble.addEventListener("webviewclosed", function(e) {
	"string"==typeof e.response && e.response.length>0 && (config=JSON.parse(e.response),localStorage.setItem("techradconfig",e.response));
	console.log("New config " + JSON.stringify(e.response));
    sendConfig();
//    readCachedData();
    if (config.CONFIG_SETCITY != "") {
//            console.log("New config fetch setcity " + config.CONFIG_SETCITY);
            fetchWeather(config.CONFIG_SETCITY, null, null);
    }
    else {
//        console.log("New config fetch GPS");
        locationWatcher = window.navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions);
    }
});
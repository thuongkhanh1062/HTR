import * as Location from 'expo-location';
import { useRouter } from 'expo-router';
import React, { useEffect, useState } from 'react';
import { ActivityIndicator, Alert, Image, ImageBackground, ScrollView, StyleSheet, Text, TouchableOpacity, View } from 'react-native';

// --- Imports ---
const AddImg = require('../../assets/images/add.png');
const AngleRightImg = require('../../assets/images/angle-right.png');
const MenuburgerImg = require('../../assets/images/menu-burger.png');
const Thermalometer = require('../../assets/images/thermometer.png');
const Humiditymeter = require('../../assets/images/humidity.png');

const FoggyImg = require('../../assets/images/foggy.png');
const StormyImg = require('../../assets/images/storm.png');
const snowyImg = require('../../assets/images/snowflake.png');
const partlycloudyImg = require('../../assets/images/clouds-and-sun-2.png');
const rainyImg = require('../../assets/images/clouds-and-drop.png');
const cloudyImg = require('../../assets/images/cloudy.png');
const sunnyImg = require('../../assets/images/sun.png');

const locationImg = require('../../assets/images/location.png');
const windSpeedImg = require('../../assets/images/windy.png');
const MoistureImg = require('../../assets/images/moisture.png');
const AirqualityImg = require('../../assets/images/airquality.png');
const SunriseImg = require('../../assets/images/sunrise.png');
const SunsetImg = require('../../assets/images/sunset.png');
const RainImg = require('../../assets/images/rain.png');

const backgroundImg = require('../../assets/backgrounds1.png');

const OPENWEATHER_API_KEY = 'fe3f1acd362a2919de02d9a4ae0cbd40';

// --- HÀM CHUYỂN ĐỔI TIMESTAMP SANG GIỜ/PHÚT ---
const convertUnixToTime = (unixTimestamp: number) => {
    const date = new Date(unixTimestamp * 1000);
    return {
        hour: date.getHours().toString().padStart(2, '0'),
        min: date.getMinutes().toString().padStart(2, '0')
    };
};

const formatCustomDate = (dateObj: Date) => {
    const options = {
        weekday: 'short',
        day: 'numeric',
        month: 'short',
        year: 'numeric',
    };
    const formattedString = dateObj.toLocaleDateString('en-GB', options);
    const parts = formattedString.split(' ');
    return `${parts[0]} ${parts[1]} ${parts[2]}, ${parts[3]}`;
};

const weatherConditions = [
    { status: "Clear", icon: sunnyImg },
    { status: "Clouds", icon: cloudyImg },
    { status: "Rain", icon: rainyImg },
    { status: "Drizzle", icon: rainyImg },
    { status: "Thunderstorm", icon: StormyImg },
    { status: "Snow", icon: snowyImg },
    { status: "Mist", icon: FoggyImg },
    { status: "Fog", icon: FoggyImg },
    { status: "Haze", icon: FoggyImg },
    { status: "Partly Cloudy", icon: partlycloudyImg },
    { status: "Stormy", icon: StormyImg },
    { status: "Foggy", icon: FoggyImg },
];

const getRandomInt = (min: number, max: number): number => {
    min = Math.ceil(min);
    max = Math.floor(max);
    return Math.floor(Math.random() * (max - min + 1)) + min;
};

const findLocalIcon = (apiMainStatus: string) => {
    const found = weatherConditions.find(wc => apiMainStatus.toLowerCase().includes(wc.status.toLowerCase()));
    return found ? found.icon : sunnyImg;
};

const convertAqiToStatus = (aqi: number) => {
    switch (aqi) {
        case 1: return "Tốt";
        case 2: return "Trung Bình";
        case 3: return "Kém";
        case 4: return "Xấu";
        case 5: return "Rất Xấu";
        default: return "N/A";
    }
};

export default function Home() {
    const router = useRouter();
    // Weather Data OpenWeatherMap
    const [loading, setLoading] = useState(true);
    const [weatherData, setWeatherData] = useState(null);
    const [errorMsg, setErrorMsg] = useState<string | null>(null);
    // get Location
    const [locationStatus, setLocationStatus] = useState('Đang tìm kiếm vị trí...');
    const [city, setCity] = useState('');
    const [country, setCountry] = useState('');
    // get Date
    const [currentTime, setCurrentTime] = useState(new Date());
    // get weather data
    const [currentTemp, setCurrentTemp] = useState(0); // API
    const [currentHumid, setCurrentHumid] = useState(0);
    const [currentWindspeed, setcurrentWindspeed] = useState(0); // API
    const [currentSunriseHour, setcurrentSunriseHour] = useState('00'); // API
    const [currentSunriseMin, setcurrentSunriseMin] = useState('00'); // API
    const [currentSunsetHour, setcurrentSunsetHour] = useState('00'); // API
    const [currentSunsetMin, setcurrentSunsetMin] = useState('00'); // API
    const [weatherInfo, setWeatherInfo] = useState({ // API
        status: "Đang tải...",
        icon: sunnyImg
    });
    // get weather data
    const [currentSoil, setCurrentSoil] = useState(0);
    const [airQuality, setAirQuality] = useState('N/A');
    const [currentRain, setCurrentRain] = useState(0);

    const fetchUserLocation = async () => {
        setLocationStatus('Đang yêu cầu quyền truy cập...');
        let { status } = await Location.requestForegroundPermissionsAsync();
        if (status !== 'granted') {
            setLocationStatus('Quyền truy cập vị trí bị từ chối.');
            Alert.alert(
                'Không có Quyền',
                'Vui lòng cấp quyền truy cập vị trí trong Cài đặt để hiển thị thông tin.'
            );
            return;
        }
        setLocationStatus('Đã có quyền. Đang lấy tọa độ...');
        try {
            let location = await Location.getCurrentPositionAsync({});
            setLocationStatus('Đã lấy tọa độ. Đang Geocode...');
            const geocodedLocation = await Location.reverseGeocodeAsync({
                latitude: location.coords.latitude,
                longitude: location.coords.longitude,
            });

            if (geocodedLocation.length > 0) {
                const firstResult = geocodedLocation[0];

                const extractedCity = firstResult.city || firstResult.subregion || 'Không xác định';
                const extractedCountry = firstResult.country || 'Không xác định';

                setCity(extractedCity);
                setCountry(extractedCountry);
                setLocationStatus('Đã tải vị trí thành công.');
            } else {
                setLocationStatus('Không tìm thấy địa chỉ cho tọa độ này.');
            }

        } catch (error) {
            setLocationStatus('Lỗi khi lấy vị trí.');
            console.error("Lỗi lấy vị trí: ", error);
        }
    };

    // --- HÀM LẤY VÀ CẬP NHẬT DỮ LIỆU THỜI TIẾT TỪ API ---
    const fetchWeatherByLocation = async () => {
        setLoading(true);
        setErrorMsg(null);

        try {
            let { status } = await Location.requestForegroundPermissionsAsync();
            if (status !== 'granted') {
                setErrorMsg("Quyền truy cập vị trí bị từ chối. Không thể lấy thời tiết.");
                setLoading(false);
                return;
            }

            let location = await Location.getCurrentPositionAsync({
                accuracy: Location.Accuracy.High
            });
            const { latitude, longitude } = location.coords;
            const weatherUrl = `https://api.openweathermap.org/data/2.5/weather?lat=${latitude}&lon=${longitude}&appid=${OPENWEATHER_API_KEY}&units=metric&lang=vi`;

            const response = await fetch(weatherUrl);
            const data = await response.json();

            if (data.cod !== 200) {
                throw new Error(data.message || "Lỗi khi gọi OpenWeatherMap API.");
            }
            setWeatherData(data);

            // --- CẬP NHẬT TỪ API ---
            setCurrentTemp(Math.round(data.main.temp));
            setCurrentHumid(data.main.humidity);

            const apiMainStatus = data.weather[0].main;
            setWeatherInfo({
                status: data.weather[0].description.charAt(0).toUpperCase() + data.weather[0].description.slice(1),
                icon: findLocalIcon(apiMainStatus)
            });

            setcurrentWindspeed(Math.round(data.wind.speed * 10) / 10);

            const sunriseTime = convertUnixToTime(data.sys.sunrise);
            const sunsetTime = convertUnixToTime(data.sys.sunset);

            setcurrentSunriseHour(sunriseTime.hour);
            setcurrentSunriseMin(sunriseTime.min);
            setcurrentSunsetHour(sunsetTime.hour);
            setcurrentSunsetMin(sunsetTime.min);

            if (data.name) setCity(data.name);
            if (data.sys.country) setCountry(data.sys.country);

            const airQualityUrl = `https://api.openweathermap.org/data/2.5/air_pollution?lat=${latitude}&lon=${longitude}&appid=${OPENWEATHER_API_KEY}`;
            const airResponse = await fetch(airQualityUrl);
            const airData = await airResponse.json();

            if (airData.list && airData.list.length > 0) {
                const aqi = airData.list[0].main.aqi;
                const aqiStatus = convertAqiToStatus(aqi);

                setAirQuality(aqiStatus);
            }
        } catch (error) {
            console.error("Lỗi toàn diện:", error);
            if (error instanceof Error) {
                setErrorMsg(`Không thể tải dữ liệu: ${error.message}`);
            } else {
                setErrorMsg("Không thể tải dữ liệu: Lỗi không xác định");
            }
        } finally {
            setLoading(false);
        }
    };

    // --- useEffect ---
    useEffect(() => {
        fetchUserLocation();
        fetchWeatherByLocation();
        setCurrentRain(getRandomInt(0, 20));

        const intervalId = setInterval(() => {
            setCurrentTime(new Date());
            const newRain = getRandomInt(0, 20);
            setCurrentRain(newRain);

        }, 1000);
        return () => {
            clearInterval(intervalId);
        };
    }, []);

    const displayDate = formatCustomDate(currentTime);
    if (loading && !errorMsg) {
        return (
            <View style={[styles.container, { backgroundColor: "#11ad45bc" }]}>
                <ActivityIndicator size="large" color="#ffffff" />
                <Text style={{ color: '#fff', marginTop: 10 }}>Đang tải dữ liệu thời tiết...</Text>
            </View>
        )
    }
    if (errorMsg) {
        return (
            <View style={[styles.container, { backgroundColor: "#c17b7bff" }]}>
                <Text style={{ color: '#fff', textAlign: 'center' }}>{errorMsg}</Text>
                <Text style={{ color: '#fff', textAlign: 'center', marginTop: 10 }} onPress={fetchWeatherByLocation}>Thử lại</Text>
            </View>
        )
    }

    const onAll = () => {
        router.push("./about");
    };

    return (

        <ImageBackground source={backgroundImg} style={styles.backgroundImage}>
            <ScrollView contentContainerStyle={styles.containerscroll}>
                <Text style={{ fontSize: 28, fontWeight: "bold", marginTop: 10, marginLeft: 50 }}>Thời tiết hôm nay</Text>
                <View style={{
                    width: "90%",
                    height: "35%",
                    backgroundColor: "#fff",
                    opacity: 0.8,
                    justifyContent: "center",
                    alignItems: "center",
                    borderRadius: 40,
                    flexDirection: "column",
                    marginLeft: "5%",
                }}>
                    <View style={{ backgroundColor: "#ffffffff", width: "80%", height: "20%", flexDirection: "column", }}>
                        <View style={{
                            flexDirection: "row",
                            justifyContent: "space-between",
                            borderRadius: 10,
                        }}>
                            <Text style={{ fontSize: 24, marginRight: 10, marginTop: 10, fontWeight: "bold" }}>Hôm nay</Text>
                            <Text style={{ fontSize: 14, marginLeft: 10, marginTop: 10, fontWeight: "bold" }}>{displayDate}</Text>
                        </View>
                    </View>
                    <View style={{ backgroundColor: "#ffffffff", width: "80%", height: "50%", flexDirection: "row", justifyContent: "space-between", marginTop: 5, }}>
                        <View style={{ flexDirection: 'column', marginBottom: 25, }}>

                            <View style={{ width: "110%", height: "30%", flexDirection: "row", }}>
                                <Image
                                    source={locationImg}
                                    style={{ width: 12, height: 12, }}
                                />
                                {locationStatus.includes('thành công') ? (
                                    <View>
                                        <Text style={{ fontSize: 14, marginLeft: 5 }}>{city}, {country}</Text>
                                    </View>
                                ) : (
                                    <View>
                                        {locationStatus.includes('Đang') && <ActivityIndicator size="small" color="#007BFF" />}
                                        <Text style={{ fontSize: 14, marginLeft: 10, }}>{locationStatus}</Text>
                                    </View>
                                )}
                            </View>
                            <View style={{ flexDirection: "row", alignItems: "center", }}>
                                <Image source={Thermalometer} style={{ width: 32, height: 32 }} />
                                <Text style={[{ fontSize: 42, fontWeight: "bold", marginLeft: 10, }]}>{currentTemp}°C</Text>
                            </View>
                            <View style={{ flexDirection: "row", alignItems: "center", }}>
                                <Image source={Humiditymeter} style={{ width: 32, height: 32 }} />
                                <Text style={[{ fontSize: 42, fontWeight: "bold", marginLeft: 10, }]}>{currentHumid}%</Text>
                            </View>
                        </View>
                        <View style={{ flexDirection: "column", alignItems: "center", marginLeft: -20 }}>
                            <Image
                                source={weatherInfo.icon}
                                style={[styles.image, { marginRight: 10, marginBottom: "10%" }]}
                            />
                            <Text style={{ fontSize: 18, marginLeft: -5 }}>{weatherInfo.status}</Text>
                        </View>
                    </View>
                    <View style={{ height: 1, backgroundColor: '#000', width: '70%', marginTop: 10, }} />
                </View>

                <View style={{ marginLeft: "5%", width: "90%", height: "50%", flexDirection: "column", marginTop: 10, marginBottom: 30 }}>
                    <View style={{ flexDirection: "row", justifyContent: "space-between", }}>
                        <Text style={[styles.title, { marginLeft: 25, }]}>Quản lý nông trại</Text>
                        <View style={{ flexDirection: "row", marginTop: 4, }}>
                            <TouchableOpacity
                                style={{ flexDirection: "row", marginTop: 4 }}
                                onPress={onAll}
                            >
                                <Text style={{ fontSize: 20, fontWeight: "bold", marginTop: 3, marginRight: 3, }}>All</Text>
                                <Image
                                    source={AngleRightImg}
                                    style={{ marginRight: 10, marginTop: 10, width: 12, height: 12, }}
                                />
                            </TouchableOpacity>
                        </View>
                    </View>
                    <View style={{
                        backgroundColor: "#ffffffff",
                        opacity: 0.8,
                        borderRadius: 50,
                        width: "100%",
                        height: "80%",
                    }}>
                        <Text style={{ marginLeft: 25, marginTop: 15, fontSize: 28, fontWeight: "bold", }}>Thông tin thời tiết</Text>
                        <View style={{
                            backgroundColor: "#ffffffff",
                            borderRadius: 50,
                            width: "100%",
                            height: "40%",
                            flexDirection: "row",
                            justifyContent: "space-between"
                        }}>
                            <View style={{ width: "30%", height: "100%", alignItems: "center", backgroundColor: "#ffffffff", }}>
                                <Text style={{ fontSize: 12, color: "#000" }}>Độ ẩm</Text>
                                <Image
                                    source={MoistureImg}
                                    style={[styles.imageIcon, { width: "50%", height: "50%", }]}
                                />
                                <Text style={{ fontSize: 18, color: "#000", fontWeight: "bold", }}>{currentHumid} %</Text>
                            </View>

                            <View style={{ width: "30%", height: "100%", alignItems: "center", }}>
                                <Text style={{ fontSize: 12, color: "#000" }}>Tốc Độ Gió</Text>
                                <Image
                                    source={windSpeedImg}
                                    style={[styles.imageIcon, { width: "50%", height: "50%", }]}
                                />
                                <Text style={{ fontSize: 18, color: "#000", fontWeight: "bold", }}>{currentWindspeed} m/s</Text>
                            </View>

                            <View style={{ width: "30%", height: "100%", alignItems: "center", }}>
                                <Text style={{ fontSize: 12, color: "#000" }}>Không khí</Text>
                                <Image
                                    source={AirqualityImg}
                                    style={[styles.imageIcon, { width: "50%", height: "50%", }]}
                                />
                                <Text style={{ fontSize: 18, color: "#000", fontWeight: "bold", }}>{airQuality}</Text>
                            </View>

                        </View>
                        <View style={{
                            backgroundColor: "#ffffffff",
                            width: "100%",
                            height: "40%",
                            flexDirection: "row",
                            justifyContent: "space-between",
                            borderRadius: 50,
                        }}>
                            <View style={{ width: "30%", height: "100%", alignItems: "center", }}>
                                <Text style={{ fontSize: 12, color: "#000" }}>Bình Minh</Text>
                                <Image
                                    source={SunriseImg}
                                    style={[styles.imageIcon, { width: "50%", height: "50%", }]}
                                />
                                <Text style={{ fontSize: 18, color: "#000", fontWeight: "bold", }}>{currentSunriseHour}:{currentSunriseMin}</Text>
                            </View>
                            <View style={{ width: "30%", height: "100%", alignItems: "center", }}>
                                <Text style={{ fontSize: 12, color: "#000" }}>Hoàng hôn</Text>
                                <Image
                                    source={SunsetImg}
                                    style={[styles.imageIcon, { width: "50%", height: "50%", }]}
                                />
                                <Text style={{ fontSize: 18, color: "#000", fontWeight: "bold", }}>{currentSunsetHour}:{currentSunsetMin}</Text>
                            </View>
                            <View style={{ width: "30%", height: "100%", alignItems: "center", }}>
                                <Text style={{ fontSize: 12, color: "#000" }}>Tỉ lệ mưa</Text>
                                <Image
                                    source={RainImg}
                                    style={[styles.imageIcon, { width: "50%", height: "50%", }]}
                                />
                                <Text style={{ fontSize: 18, color: "#000", fontWeight: "bold", }}>{currentRain}%</Text>
                            </View>
                        </View>
                    </View>
                </View>
            </ScrollView>
        </ImageBackground>
    )
}
const styles = StyleSheet.create({
    backgroundImage: {
        flex: 1,
    },
    gridContainer: {
        flexDirection: 'row',
        flexWrap: 'wrap',
        justifyContent: 'space-around',
        padding: 5,
    },
    container: {
        flex: 1,
        justifyContent: "center",
        padding: 20,
        alignItems: "center",
    },
    containerscroll: {
        flex: 1,
        justifyContent: "center",
    },
    nodecontainer: {
        width: "90%",
        height: "40%",
        justifyContent: "center",
        alignItems: "center",
        borderRadius: 20,
        borderWidth: 2,
        shadowOpacity: 0.5,
    },
    containerlittletitle: {
        flexDirection: "row",
        justifyContent: "space-between",
        alignItems: "center",
        margin: 20,
    },
    datacontainer: {
        backgroundColor: "#fff",
        opacity: 0.5,
        borderRadius: 20,
        borderWidth: 2,
        shadowOpacity: 0.5,
    },
    line: {
        height: 1,
        width: "80%",
        alignContent: "center",
        backgroundColor: "#000",
        justifyContent: "center",
    },
    title: {
        fontSize: 28,
        fontWeight: "bold",
        marginBottom: 20,
    },
    button: {
        width: "45%",
        backgroundColor: "#11ad45bc",
        padding: 10,
        borderRadius: 10,
        alignItems: "center"
    },
    buttonText: {
        color: "#fff",
        fontWeight: "bold"
    },
    image: {
        width: 80,
        height: 80,
    },
    imageIcon: {
        width: "50%",
        height: "50%",
    },
});
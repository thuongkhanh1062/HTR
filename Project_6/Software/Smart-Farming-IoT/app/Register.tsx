import { Ionicons } from "@expo/vector-icons";
import { router } from "expo-router";
import { child, get, ref, set } from 'firebase/database';
import React, { useState } from "react";
import {
    Alert,
    ImageBackground,
    KeyboardAvoidingView,
    Platform,
    ScrollView,
    StyleSheet,
    Text,
    TextInput,
    TouchableOpacity,
    View,
    useWindowDimensions
} from "react-native";
import { database } from 'E:/HTR/Project_6/Software/Smart-Farming-IoT/firebaseConfig.js';

const backgroundImg = require('../assets/backgrounds1.png');

const FIREBASE_USER_PATH = 'smart_farm_iot/user_data';
const dbRef = ref(database);

export default function RegisterScreen() {
    const { width, height } = useWindowDimensions();

    const [phone, setPhone] = useState("");
    const [name, setName] = useState("");
    const [email, setEmail] = useState("");
    const [password, setPassword] = useState("");
    const [showPassword, setShowPassword] = useState(false);
    const [registeredUserId, setRegisteredUserId] = useState<string | null>(null);

    const onBackToLogin = () => {
        setRegisteredUserId(null);
        router.replace("./login");
    };

    const handleSuccessfulRegistration = (newUserId: string, userName: string) => {
        setRegisteredUserId(newUserId);
        Alert.alert(
            "Đăng ký thành công 🎉",
            `Chào mừng ${userName}! Tài khoản đã được tạo.\n\nID đăng nhập của bạn là: ${newUserId}`,
            [
                { text: "Đăng nhập ngay", onPress: onBackToLogin }
            ]
        );
    };

    const onRegister = async () => {
        if (!email || !password || !name || !phone) {
            Alert.alert("Lỗi", "Vui lòng điền đầy đủ tất cả các trường: Email, Tên, Số điện thoại và Mật khẩu.");
            return;
        }

        setRegisteredUserId(null);

        try {
            const snapshot = await get(child(dbRef, FIREBASE_USER_PATH));

            let nextUserIdIndex = 1;
            let isDuplicate = false;

            if (snapshot.exists()) {
                const existingUsers = snapshot.val();
                let maxIndex = 0;

                for (const key in existingUsers) {
                    if (existingUsers.hasOwnProperty(key)) {
                        const user = existingUsers[key];
                        if (user.name === name) {
                            Alert.alert("Lỗi", `Tên đăng nhập "${name}" đã tồn tại. Vui lòng chọn tên khác.`);
                            isDuplicate = true;
                            break;
                        }
                        if (user.email === email) {
                            Alert.alert("Lỗi", `Email "${email}" đã được đăng ký. Vui lòng sử dụng Email khác.`);
                            isDuplicate = true;
                            break;
                        }
                        if (user.phone === phone) {
                            Alert.alert("Lỗi", `Số điện thoại "${phone}" đã được đăng ký. Vui lòng sử dụng SĐT khác.`);
                            isDuplicate = true;
                            break;
                        }

                        const match = key.match(/user_(\d+)/);
                        if (match) {
                            const index = parseInt(match[1]);
                            if (index > maxIndex) {
                                maxIndex = index;
                            }
                        }
                    }
                }
                nextUserIdIndex = maxIndex + 1;
            }

            if (isDuplicate) {
                return;
            }

            const newUserId = `user_${nextUserIdIndex}`;
            const newUserPath = `${FIREBASE_USER_PATH}/${newUserId}`;

            const newUserData = {
                name: name,
                email: email,
                phone: phone,
                password: password,
                created_at: new Date().toISOString(),
            };

            await set(ref(database, newUserPath), newUserData);

            handleSuccessfulRegistration(newUserId, name);
            setPhone('');
            setName('');
            setEmail('');
            setPassword('');

        } catch (error) {
            console.error("Lỗi trong quá trình đăng ký Firebase: ", error);
            Alert.alert("Lỗi hệ thống", "Đã xảy ra lỗi khi kết nối hoặc ghi dữ liệu lên Firebase.");
        }
    };

    const isLandscape = width > height;
    const registerBoxWidth = isLandscape ? '55%' : '85%'; 
    const minHeightCalc = isLandscape ? height * 0.9 : 550;

    return (
        <ImageBackground
            source={backgroundImg}
            style={styles.backgroundImage}
            imageStyle={{ opacity: 1 }}
        >
            <KeyboardAvoidingView
                style={styles.keyboardAvoidingView}
                behavior={Platform.OS === "ios" ? "padding" : "height"}
                keyboardVerticalOffset={Platform.OS === "ios" ? 0 : (isLandscape ? -100 : 20)}
            >
                <ScrollView contentContainerStyle={styles.scrollContent}>
                    <View style={styles.container}>
                        <View style={[styles.registerBox, { width: registerBoxWidth, minHeight: minHeightCalc }]}>
                            <Text style={styles.title}>REGISTER</Text>

                            {registeredUserId && (
                                <View style={[styles.inputWrapper, styles.userIdDisplay]}>
                                    <Text style={styles.userIdText}>
                                        ID CỦA BẠN: {registeredUserId}
                                    </Text>
                                </View>
                            )}

                            <View style={styles.inputWrapper}>
                                <TextInput style={styles.textInput} placeholder="Email"
                                    placeholderTextColor="#999"
                                    onChangeText={setEmail}
                                    value={email}
                                    keyboardType="email-address"
                                    autoCapitalize="none"
                                />
                            </View>
                            <View style={styles.inputWrapper}>
                                <TextInput style={styles.textInput} placeholder="Name (Tên đăng nhập)"
                                    placeholderTextColor="#999"
                                    onChangeText={setName}
                                    value={name}
                                />
                            </View>

                            <View style={styles.inputWrapper}>
                                <TextInput style={styles.textInput} placeholder="Số điện thoại"
                                    placeholderTextColor="#999"
                                    onChangeText={setPhone}
                                    value={phone}
                                    keyboardType="phone-pad"
                                />
                            </View>

                            <View style={styles.passwordContainer}>
                                <TextInput
                                    style={styles.passwordInput}
                                    placeholder="Password"
                                    placeholderTextColor="#999"
                                    secureTextEntry={!showPassword}
                                    onChangeText={setPassword}
                                    value={password}
                                />
                                <TouchableOpacity
                                    onPress={() => setShowPassword(!showPassword)}
                                    style={styles.iconWrapper}
                                >
                                    <Ionicons
                                        name={showPassword ? "eye-off" : "eye"}
                                        size={24}
                                        color="#555"
                                    />
                                </TouchableOpacity>
                            </View>

                            <View style={styles.btnlogincontainer}>
                                <TouchableOpacity style={styles.button} onPress={onBackToLogin}>
                                    <Text style={styles.buttonText}>Back</Text>
                                </TouchableOpacity>

                                <TouchableOpacity style={styles.button} onPress={onRegister}>
                                    <Text style={styles.buttonText}>Register</Text>
                                </TouchableOpacity>

                            </View>
                        </View>
                    </View>
                </ScrollView>
            </KeyboardAvoidingView>
        </ImageBackground>
    );
}

const styles = StyleSheet.create({
    backgroundImage: {
        flex: 1,
        width: '100%',
        alignItems: "center",
        justifyContent: "center",
    },
    keyboardAvoidingView: {
        flex: 1,
        width: '100%',
    },
    scrollContent: {
        flexGrow: 1,
        justifyContent: "center",
        alignItems: "center",
        paddingVertical: 20,
    },
    container: {
        flex: 1,
        justifyContent: "center",
        alignItems: "center",
        width: '100%',
    },
    registerBox: {
        justifyContent: "space-around",
        backgroundColor: "#ffffffff",
        borderRadius: 50,
        alignItems: "center",
        paddingVertical: 30,
        paddingHorizontal: 10,
    },
    title: {
        color: "#11ad45bc",
        fontSize: 28,
        fontWeight: "bold",
        marginBottom: 20,
        textAlign: "center"
    },
    userIdDisplay: {
        backgroundColor: '#e0ffe0',
        borderColor: '#11ad45bc',
        marginBottom: 20,
    },
    userIdText: {
        fontSize: 16, 
        fontWeight: 'bold', 
        color: '#11ad45bc',
        textAlign: 'center',
    },
    inputWrapper: {
        width: "90%",
        height: 50,
        borderWidth: 1,
        marginBottom: 15,
        borderRadius: 25,
        paddingHorizontal: 20,
        borderColor: "#ccc",
        justifyContent: "center",
        backgroundColor: "#ffffffff",
    },
    textInput: {
        fontSize: 16,
        flex: 1,
    },
    passwordContainer: {
        width: "90%",
        height: 50,
        flexDirection: "row",
        alignItems: "center",
        borderWidth: 1,
        borderColor: "#ccc",
        borderRadius: 25,
        backgroundColor: "#fff",
        paddingHorizontal: 20,
        marginBottom: 15,
    },
    passwordInput: {
        flex: 1,
        fontSize: 16,
        color: "#000",
        height: '100%',
    },
    iconWrapper: {
        paddingLeft: 10,
    },
    btnlogincontainer: {
        width: "90%",
        height: 70,
        justifyContent: "space-around",
        flexDirection: "row",
        alignItems: "center",
        marginTop: 10,
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
        fontWeight: "bold",
        fontSize: 18,
    },
});
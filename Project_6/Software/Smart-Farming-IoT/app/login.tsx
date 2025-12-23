import { Ionicons } from "@expo/vector-icons";
import AsyncStorage from '@react-native-async-storage/async-storage';
import { router } from "expo-router";
import { child, get, ref } from 'firebase/database';
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

/**
 * Lưu userId của người dùng đăng nhập thành công vào AsyncStorage.
 * @param userId ID người dùng.
 */
const saveUserId = async (userId: string): Promise<void> => {
    try {
        await AsyncStorage.setItem('loggedInUserId', userId);
        console.log(`[AUTH] Đã lưu User ID: ${userId}`);
    } catch (e) {
        console.error("Lỗi khi lưu User ID:", e);
    }
};

export default function LoginScreen() {
    const { width, height } = useWindowDimensions();

    const [loginIdentifier, setLoginIdentifier] = useState("");
    const [password, setPassword] = useState("");
    const [showPassword, setShowPassword] = useState(false);
    const onLogin = async () => {
        if (!loginIdentifier || !password) {
            Alert.alert("Lỗi", "Vui lòng nhập định danh (ID/Email/SĐT/Tên) và Mật khẩu.");
            return;
        }

        try {
            const snapshot = await get(child(dbRef, FIREBASE_USER_PATH));

            if (snapshot.exists()) {
                const usersData = snapshot.val();
                let isLoginSuccessful = false;
                let loggedInUserId: string | null = null;

                const identifierLower = loginIdentifier.toLowerCase().trim();

                for (const userId in usersData) {
                    if (usersData.hasOwnProperty(userId)) {
                        const userData = usersData[userId];

                        const storedName = userData.name ? userData.name.toLowerCase().trim() : '';
                        const storedEmail = userData.email ? userData.email.toLowerCase().trim() : '';
                        const storedPhone = userData.phone ? userData.phone.trim() : '';

                        const identifierMatches = (
                            identifierLower === userId.toLowerCase() ||
                            identifierLower === storedName ||
                            identifierLower === storedEmail ||
                            identifierLower === storedPhone
                        );

                        if (identifierMatches && password === userData.password) {
                            isLoginSuccessful = true;
                            loggedInUserId = userId;
                            break;
                        }
                    }
                }

                if (isLoginSuccessful && loggedInUserId) {
                    await saveUserId(loggedInUserId);
                    router.replace("./home");
                } else {
                    Alert.alert("Lỗi", "Sai thông tin đăng nhập hoặc Mật khẩu! Vui lòng thử lại.");
                }
            } else {
                Alert.alert("Lỗi", "Hệ thống chưa có người dùng nào được đăng ký.");
            }
        } catch (error) {
            console.error("Lỗi khi đọc dữ liệu Firebase: ", error);
            Alert.alert("Lỗi", "Không thể kết nối với Firebase. Vui lòng kiểm tra mạng hoặc cấu hình.");
        }
    };

    const onRegister = () => {
        router.replace("./Register");
    };
    
    const onRestore = () => {
        router.replace("./restore");
    };
    const isLandscape = width > height;
    const loginBoxHeight = isLandscape ? height * 0.8 : height * 0.5;
    const loginBoxWidth = isLandscape ? '50%' : '80%';
    const keyboardOffset = Platform.OS === "ios" ? 0 : (isLandscape ? -100 : 20);

    return (
        <ImageBackground
            source={backgroundImg}
            style={styles.imageBackground}
            imageStyle={{ opacity: 1 }}
        >
            <KeyboardAvoidingView
                style={styles.keyboardAvoidingView}
                behavior={Platform.OS === "ios" ? "padding" : "height"}
                keyboardVerticalOffset={keyboardOffset} 
            >
                <ScrollView contentContainerStyle={styles.scrollContent}>
                    <View style={styles.container}>
                        <View style={[styles.loginBox, { height: loginBoxHeight, width: loginBoxWidth }]}>
                            <Text style={[styles.title, { color: "#11ad45bc" }]}>LOGIN</Text>

                            <View style={styles.inputWrapper}>
                                <TextInput
                                    style={styles.textInput}
                                    placeholder="User name"
                                    placeholderTextColor="#999"
                                    onChangeText={setLoginIdentifier}
                                    autoCapitalize="none"
                                />
                            </View>

                            <View style={styles.passwordWrapper}>
                                <TextInput
                                    style={styles.passwordInput}
                                    placeholder="Password"
                                    placeholderTextColor="#999"
                                    secureTextEntry={!showPassword}
                                    onChangeText={setPassword}
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

                            <View style={styles.forgotPasswordContainer}>
                                <TouchableOpacity onPress={onRestore}>
                                    <Text style={styles.forgotPasswordText}>
                                        Forgot Password?
                                    </Text>
                                </TouchableOpacity>
                            </View>

                            <View style={styles.btnlogincontainer}>
                                <TouchableOpacity style={styles.button} onPress={onRegister}>
                                    <Text style={styles.buttonText}>Register</Text>
                                </TouchableOpacity>

                                <TouchableOpacity style={styles.button} onPress={onLogin}>
                                    <Text style={styles.buttonText}>Login</Text>
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
    imageBackground: {
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
    },
    container: {
        flex: 1,
        justifyContent: "center",
        alignItems: "center",
        width: '100%',
    },
    loginBox: {
        justifyContent: "center",
        backgroundColor: "#ffffffff",
        borderRadius: 50,
        alignItems: "center",
        paddingVertical: 30,
    },
    title: {
        fontSize: 28,
        fontWeight: "bold",
        marginBottom: 20,
        textAlign: "center"
    },
    inputWrapper: {
        width: "90%",
        height: 50,
        borderWidth: 1,
        marginBottom: 15,
        borderRadius: 25,
        paddingHorizontal: 10,
        borderColor: "#ccc",
        justifyContent: "center",
        backgroundColor: "#ffffffff",
    },
    textInput: {
        fontSize: 16,
        marginLeft: 5,
        flex: 1,
    },
    passwordWrapper: {
        width: "90%",
        height: 50,
        flexDirection: "row",
        alignItems: "center",
        borderWidth: 1,
        borderColor: "#ccc",
        borderRadius: 25,
        backgroundColor: "#fff",
        paddingHorizontal: 10,
        marginBottom: 5,
    },
    passwordInput: {
        flex: 1,
        fontSize: 16,
        color: "#000",
        marginLeft: 5,
        height: '100%'
    },
    iconWrapper: {
        paddingLeft: 10,
    },
    forgotPasswordContainer: {
        width: "90%",
        alignItems: "flex-end",
        paddingRight: 15,
        marginBottom: 20,
    },
    forgotPasswordText: {
        color: "#11ad45bc",
        fontWeight: "bold",
        fontSize: 12
    },
    btnlogincontainer: {
        width: "90%",
        height: 70,
        justifyContent: "space-around",
        flexDirection: "row",
        alignItems: "center",
        paddingHorizontal: 10,
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
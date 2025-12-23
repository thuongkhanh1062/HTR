import { Ionicons } from "@expo/vector-icons";
import { router } from "expo-router";
import { child, get, ref, update } from 'firebase/database';
import React, { useState } from "react";
import {
    Alert,
    ImageBackground,
    StyleSheet,
    Text,
    TextInput,
    TouchableOpacity,
    View,
    KeyboardAvoidingView,
    Platform,
    ScrollView,
    useWindowDimensions // <-- Import useWindowDimensions
} from "react-native";
import { database } from 'E:/HTR/Project_6/Software/Smart-Farming-IoT/firebaseConfig.js';

const backgroundImg = require('../assets/backgrounds1.png');
const FIREBASE_USER_PATH = 'smart_farm_iot/user_data';
const dbRef = ref(database);

const generateOTP = () => {
    return Math.floor(100000 + Math.random() * 900000).toString();
};

export default function RestoreScreen() {
    const { width, height } = useWindowDimensions();
    const isLandscape = width > height;

    const [currentStage, setCurrentStage] = useState(1);

    // Stage 1 & 2: Identifier and OTP
    const [identifier, setIdentifier] = useState("");
    const [foundUserId, setFoundUserId] = useState<string | null>(null);
    const [storedOTP, setStoredOTP] = useState<string | null>(null);
    const [inputOTP, setInputOTP] = useState("");

    // Stage 3: New Password
    const [newPassword, setNewPassword] = useState("");
    const [confirmPassword, setConfirmPassword] = useState("");
    const [showPassword, setShowPassword] = useState(false);

    const onBackToLogin = () => {
        router.replace("./login");
    };

    /**
     * Giai đoạn 1: Tìm kiếm tài khoản và Giả lập gửi OTP.
     */
    const onSearchAccountAndSendOTP = async () => {
        if (!identifier) {
            Alert.alert("Lỗi", "Vui lòng nhập Email hoặc Số điện thoại để khôi phục.");
            return;
        }

        try {
            const snapshot = await get(child(dbRef, FIREBASE_USER_PATH));

            if (snapshot.exists()) {
                const usersData = snapshot.val();
                let userIdFound: string | null = null;
                const identifierLower = identifier.toLowerCase().trim();

                // 1. Tìm kiếm User
                for (const userId in usersData) {
                    const userData = usersData[userId];
                    const storedEmail = userData.email ? userData.email.toLowerCase().trim() : '';
                    const storedPhone = userData.phone ? userData.phone.trim() : '';

                    if (identifierLower === storedEmail || identifierLower === storedPhone) {
                        userIdFound = userId;
                        break;
                    }
                }

                if (userIdFound) {
                    // 2. Tạo và Lưu OTP (Giả lập)
                    const newOTP = generateOTP();
                    setFoundUserId(userIdFound);
                    setStoredOTP(newOTP);
                    setInputOTP("");

                    Alert.alert(
                        "Mã OTP đã được gửi",
                        `Tài khoản liên kết với '${identifier}' đã được tìm thấy.\n\n[MÃ GIẢ LẬP]: ${newOTP}\n\nVui lòng nhập mã này để xác thực.`,
                        [{ text: "OK", onPress: () => setCurrentStage(2) }]
                    );
                } else {
                    Alert.alert("Lỗi", "Không tìm thấy tài khoản nào khớp.");
                }
            } else {
                Alert.alert("Lỗi", "Hệ thống chưa có người dùng nào được đăng ký.");
            }
        } catch (error) {
            console.error("Lỗi khi đọc dữ liệu Firebase: ", error);
            Alert.alert("Lỗi hệ thống", "Không thể kết nối với Firebase.");
        }
    };

    /**
     * Giai đoạn 2: Xác thực OTP.
     */
    const onVerifyOTP = () => {
        if (!inputOTP || !storedOTP) {
            Alert.alert("Lỗi", "Vui lòng nhập mã OTP.");
            return;
        }

        if (inputOTP === storedOTP) {
            Alert.alert("Thành công", "Mã xác thực chính xác! Bây giờ bạn có thể đặt mật khẩu mới.");
            setCurrentStage(3);
            setStoredOTP(null);
            setInputOTP("");
        } else {
            Alert.alert("Lỗi", "Mã OTP không hợp lệ. Vui lòng thử lại.");
        }
    };

    /**
     * Giai đoạn 3: Cập nhật mật khẩu mới.
     */
    const onResetPassword = async () => {
        if (!foundUserId) {
            Alert.alert("Lỗi", "Quy trình xác thực chưa hoàn tất.");
            return;
        }

        if (!newPassword || newPassword.length < 6) {
            Alert.alert("Lỗi", "Mật khẩu mới phải có ít nhất 6 ký tự.");
            return;
        }

        if (newPassword !== confirmPassword) {
            Alert.alert("Lỗi", "Mật khẩu mới và xác nhận mật khẩu không khớp.");
            return;
        }

        try {
            const userRef = ref(database, `${FIREBASE_USER_PATH}/${foundUserId}`);

            await update(userRef, {
                password: newPassword
            });

            Alert.alert("Thành công 🎉", "Mật khẩu đã được khôi phục thành công. Vui lòng đăng nhập lại.");

            // Reset trạng thái và chuyển về màn hình đăng nhập
            setFoundUserId(null);
            setNewPassword("");
            setConfirmPassword("");
            setIdentifier("");
            setCurrentStage(1);
            router.replace("./login");

        } catch (error) {
            console.error("Lỗi khi cập nhật mật khẩu Firebase: ", error);
            Alert.alert("Lỗi hệ thống", "Đã xảy ra lỗi khi cập nhật mật khẩu.");
        }
    };

    // --- RENDER HÀM XỬ LÝ THEO STAGE ---
    const renderContent = () => {
        switch (currentStage) {
            case 1:
                return (
                    <>
                        <Text style={styles.instructionText}>
                            Nhập **Email** hoặc **Số điện thoại** liên kết với tài khoản:
                        </Text>
                        <View style={styles.inputWrapper}>
                            <TextInput
                                style={styles.textInput}
                                placeholder="Email hoặc Số điện thoại"
                                placeholderTextColor="#999"
                                onChangeText={setIdentifier}
                                value={identifier}
                                autoCapitalize="none"
                                keyboardType="email-address"
                            />
                        </View>
                        <TouchableOpacity style={styles.buttonGreen} onPress={onSearchAccountAndSendOTP}>
                            <Text style={styles.buttonText}>Tìm Tài Khoản & Gửi Mã</Text>
                        </TouchableOpacity>
                    </>
                );

            case 2:
                return (
                    <>
                        <Text style={styles.instructionText}>
                            Mã xác thực (OTP) đã được gửi tới tài khoản **{identifier}**.
                        </Text>
                        <Text style={[styles.instructionText, { fontWeight: 'bold' }]}>
                            Vui lòng kiểm tra và nhập mã:
                        </Text>
                        <View style={styles.inputWrapper}>
                            <TextInput
                                style={styles.textInput}
                                placeholder="Nhập Mã OTP (6 chữ số)"
                                placeholderTextColor="#999"
                                onChangeText={setInputOTP}
                                value={inputOTP}
                                keyboardType="number-pad"
                                maxLength={6}
                            />
                        </View>
                        <TouchableOpacity style={styles.buttonGreen} onPress={onVerifyOTP}>
                            <Text style={styles.buttonText}>Xác thực Mã OTP</Text>
                        </TouchableOpacity>
                    </>
                );

            case 3:
                return (
                    <>
                        <Text style={styles.instructionText}>
                            Tài khoản được xác thực thành công. Đặt mật khẩu mới cho ID: **{foundUserId}**
                        </Text>

                        {/* Mật khẩu mới */}
                        <View style={styles.passwordContainer}>
                            <TextInput
                                style={styles.passwordInput}
                                placeholder="Mật khẩu mới (ít nhất 6 ký tự)"
                                placeholderTextColor="#999"
                                secureTextEntry={!showPassword}
                                onChangeText={setNewPassword}
                                value={newPassword}
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

                        {/* Xác nhận mật khẩu mới */}
                        <View style={styles.passwordContainer}>
                            <TextInput
                                style={styles.passwordInput}
                                placeholder="Xác nhận mật khẩu mới"
                                placeholderTextColor="#999"
                                secureTextEntry={!showPassword}
                                onChangeText={setConfirmPassword}
                                value={confirmPassword}
                            />
                        </View>

                        <TouchableOpacity style={styles.buttonGreen} onPress={onResetPassword}>
                            <Text style={styles.buttonText}>Đặt Lại Mật Khẩu</Text>
                        </TouchableOpacity>
                    </>
                );
            default:
                return <Text>Loading...</Text>;
        }
    };

    // Logic responsive cho restoreBox
    // Chiều rộng hộp: 85% dọc, 55% ngang
    const restoreBoxWidth = isLandscape ? '55%' : '85%';
    // Chiều cao tối thiểu: Đảm bảo có đủ không gian cho 3 stage
    const minHeightCalc = isLandscape ? height * 0.9 : 350;

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
                        <View style={[styles.restoreBox, { width: restoreBoxWidth, minHeight: minHeightCalc }]}>
                            <Text style={styles.title}>KHÔI PHỤC MẬT KHẨU</Text>

                            {renderContent()}

                            <View style={styles.btnlogincontainer}>
                                <TouchableOpacity style={styles.buttonGrey} onPress={onBackToLogin}>
                                    <Text style={styles.buttonText}>Quay Lại Đăng Nhập</Text>
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
    restoreBox: {
        // width và minHeight được đặt linh hoạt trong component
        justifyContent: "center",
        backgroundColor: "#ffffffff",
        borderRadius: 30,
        alignItems: "center",
        paddingVertical: 30,
        paddingHorizontal: 15,
        shadowColor: "#000",
        shadowOffset: { width: 0, height: 2 },
        shadowOpacity: 0.1,
        shadowRadius: 5,
        elevation: 5,
    },
    title: {
        color: "#11ad45bc",
        fontSize: 24,
        fontWeight: "bold",
        marginBottom: 20,
        textAlign: "center"
    },
    instructionText: {
        fontSize: 15,
        color: '#333',
        marginBottom: 10,
        textAlign: 'center',
        paddingHorizontal: 10,
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
        marginTop: 20,
        alignItems: 'center', // Căn giữa nút Quay Lại
    },
    buttonGreen: {
        width: "90%",
        backgroundColor: "#11ad45bc",
        padding: 12,
        borderRadius: 25,
        alignItems: "center",
        marginTop: 10,
    },
    buttonGrey: {
        width: "90%",
        backgroundColor: "#999",
        padding: 12,
        borderRadius: 25,
        alignItems: "center",
        marginTop: 10,
        alignSelf: 'center',
    },
    buttonText: {
        color: "#fff",
        fontWeight: "bold",
        fontSize: 18,
    },
});
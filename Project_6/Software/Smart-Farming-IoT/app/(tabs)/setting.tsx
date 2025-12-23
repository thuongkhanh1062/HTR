import AsyncStorage from '@react-native-async-storage/async-storage';
import { useRouter } from 'expo-router';
import { child, get, ref } from 'firebase/database';
import React, { useEffect, useState } from 'react';
import { ActivityIndicator, Alert, Image, ImageBackground, StyleSheet, Text, TouchableOpacity, View } from 'react-native';
import { database } from 'E:/HTR/Project_6/Software/Smart-Farming-IoT/firebaseConfig.js';

const PlaceholderAvatar = require('E:/HTR/Project_6/Software/Smart-Farming-IoT/assets/images/user.png');
const backgroundImg = require('../../assets/backgrounds1.png');
const FIREBASE_USER_ROOT_PATH = `smart_farm_iot/user_data`;
const dbRef = ref(database);

const getUserId = async (): Promise<string | null> => {
    try {
        const userId = await AsyncStorage.getItem('loggedInUserId');
        return userId;
    } catch (error) {
        console.error("Lỗi khi đọc User ID từ AsyncStorage:", error);
        return null;
    }
};

const UserInfoContainer = ({ name, accountName, avatarSource, isLoading }: { name: string; accountName: string; avatarSource: any; isLoading: boolean }) => (
    <View style={styles.userInfoContainer}>
        {isLoading ? (
            <ActivityIndicator size="large" color="#11ad45bc" style={styles.avatar} />
        ) : (
            <Image
                source={avatarSource}
                style={styles.avatar}
            />
        )}
        <View style={styles.textContainer}>
            <Text style={styles.userName}>
                {isLoading ? 'Đang tải...' : name || 'Chưa cập nhật'}
            </Text>
            <Text style={styles.accountName}>
                {isLoading ? 'Đang tải...' : accountName || 'N/A'}
            </Text>
        </View>
    </View>
);

const SettingRow = ({ title, targetPath, router, isLogout = false, onAction = null }: { title: string; targetPath: string | null; router: any; isLogout?: boolean; onAction?: (() => void) | null }) => (
    <TouchableOpacity
        style={[styles.settingRow, isLogout && styles.logoutRow]}
        onPress={() => {
            if (isLogout && onAction) {
                onAction();
            } else if (targetPath) {
                router.push(targetPath);
            }
        }}
    >
        <Text style={[styles.settingTitle, isLogout && styles.logoutTitle]}>{title}</Text>
        {!isLogout && <Text style={styles.arrow}>&gt;</Text>}
    </TouchableOpacity>
);

const Setting = () => {
    const router = useRouter();
    const [userData, setUserData] = useState({ name: '', email: '', password: '' });
    const [isLoading, setIsLoading] = useState(true);
    const [currentUserId, setCurrentUserId] = useState<string | null>(null);

    const fetchUserData = async () => {
        setIsLoading(true);
        try {
            const foundUserId = await getUserId();
            if (!foundUserId) {
                Alert.alert("Phiên hết hạn", "Người dùng chưa đăng nhập hoặc phiên đã hết hạn.");
                router.replace("/login");
                setIsLoading(false);
                return;
            }

            setCurrentUserId(foundUserId);

            const FIREBASE_USER_PATH = `${FIREBASE_USER_ROOT_PATH}/${foundUserId}`;
            const snapshot = await get(child(dbRef, FIREBASE_USER_PATH));

            if (snapshot.exists()) {
                const data = snapshot.val();
                setUserData(data);
            } else {
                Alert.alert("Lỗi", `Không tìm thấy dữ liệu cho User ID: ${foundUserId}`);
            }
        } catch (error) {
            console.error("Lỗi khi đọc dữ liệu Firebase:", error);
            Alert.alert("Lỗi hệ thống", "Không thể tải dữ liệu người dùng.");
        } finally {
            setIsLoading(false);
        }
    };

    useEffect(() => {
        fetchUserData();
    }, []);

    const handleGoToPersonalInfo = () => {
        router.push("/settings/personinfo");
    };

    const handleLogout = () => {
        Alert.alert(
            "Xác nhận đăng xuất",
            "Bạn có chắc chắn muốn đăng xuất khỏi tài khoản này?",
            [
                {
                    text: "Hủy",
                    style: "cancel"
                },
                {
                    text: "Đăng xuất",
                    onPress: async () => {
                        try {
                            await AsyncStorage.removeItem('loggedInUserId');
                            console.log("[AUTH] Đã xóa User ID đã lưu.");
                            router.replace("/login");
                        } catch (e) {
                            console.error("Lỗi khi xóa User ID:", e);
                            Alert.alert("Lỗi", "Không thể đăng xuất. Vui lòng thử lại.");
                        }
                    }
                }
            ]
        );
    };

    return (
        <ImageBackground source={backgroundImg} style={styles.backgroundImage}>
            <Text style={styles.header}>CÀI ĐẶT TÀI KHOẢN</Text>

            <TouchableOpacity onPress={handleGoToPersonalInfo} activeOpacity={0.8}>
                <UserInfoContainer
                    name={userData.name}
                    accountName={userData.email}
                    avatarSource={PlaceholderAvatar}
                    isLoading={isLoading}
                />
            </TouchableOpacity>

            <View style={styles.settingList}>
                <SettingRow
                    title="Quản lý Node"
                    targetPath="/settings/managenode"
                    router={router}
                />
                <SettingRow
                    title="Cài đặt Node"
                    targetPath="/settings/nodeconfig"
                    router={router}
                />
                <SettingRow
                    title="Thông tin phần mềm"
                    targetPath="/settings/appinfo"
                    router={router}
                />
            </View>

            <View style={styles.logoutSection}>
                <SettingRow
                    title="Đăng xuất"
                    targetPath={null}
                    router={router}
                    isLogout={true}
                    onAction={handleLogout}
                />
            </View>
        </ImageBackground>
    );
};

export default Setting;

const styles = StyleSheet.create({
    backgroundImage: {
        flex: 1,
    },
    container: {
        flex: 1,
        backgroundColor: '#f5f5f5',
        padding: 15,
    },
    header: {
        fontSize: 24,
        fontWeight: 'bold',
        textAlign: 'center',
        marginVertical: 10,
        color: '#333',
    },
    userInfoContainer: {
        flexDirection: 'row',
        alignItems: 'center',
        backgroundColor: '#fff',
        padding: 20,
        borderRadius: 10,
        marginBottom: 25,
        shadowColor: '#000',
        shadowOffset: { width: 0, height: 2 },
        shadowOpacity: 0.1,
        shadowRadius: 3,
        elevation: 5,
        borderLeftWidth: 5,
        borderLeftColor: '#11ad45bc',
    },
    avatar: {
        width: 70,
        height: 70,
        borderRadius: 35,
        marginRight: 20,
        borderWidth: 2,
        borderColor: '#eee',
    },
    textContainer: {
        flex: 1,
        justifyContent: 'center',
    },
    userName: {
        fontSize: 17,
        fontWeight: 'bold',
        color: '#333',
        marginBottom: 3,
    },
    accountName: {
        fontSize: 15,
        color: '#666',
        fontStyle: 'italic',
    },
    settingList: {
        backgroundColor: '#fff',
        borderRadius: 10,
        shadowColor: '#000',
        shadowOffset: { width: 0, height: 2 },
        shadowOpacity: 0.1,
        shadowRadius: 3,
        elevation: 5,
        marginBottom: 20,
        overflow: 'hidden',
    },
    settingRow: {
        flexDirection: 'row',
        justifyContent: 'space-between',
        alignItems: 'center',
        paddingVertical: 15,
        paddingHorizontal: 20,
        borderBottomWidth: 1,
        borderBottomColor: '#eee',
    },
    settingTitle: {
        fontSize: 17,
        color: '#333',
        fontWeight: '500',
    },
    arrow: {
        fontSize: 18,
        color: '#aaa',
        fontWeight: 'bold',
    },
    logoutSection: {
        backgroundColor: '#fff',
        borderRadius: 10,
        shadowColor: '#000',
        shadowOffset: { width: 0, height: 2 },
        shadowOpacity: 0.1,
        shadowRadius: 3,
        elevation: 5,
        overflow: 'hidden',
    },
    logoutRow: {
        borderBottomWidth: 0,
        paddingVertical: 15,
    },
    logoutTitle: {
        color: '#ff3333',
        fontWeight: 'bold',
        textAlign: 'center',
        width: '100%',
    }
});
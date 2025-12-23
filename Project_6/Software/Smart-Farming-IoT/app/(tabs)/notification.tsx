import { Ionicons } from '@expo/vector-icons';
import { useRouter } from 'expo-router';
import { limitToLast, onValue, orderByChild, push, query, ref, set } from 'firebase/database';
import React, { useEffect, useState } from 'react';
import { ActivityIndicator, Alert, ImageBackground, ScrollView, StyleSheet, Text, TouchableOpacity, View } from 'react-native';

import { database } from 'E:/HTR/Project_6/Software/Smart-Farming-IoT/firebaseConfig.js';

const backgroundImg = require('../../assets/backgrounds1.png');

// --- CẤU HÌNH FIREBASE---
const FIREBASE_LOG_PATH = 'smart_farm_iot/logs/notifications';
const notificationRef = ref(database, FIREBASE_LOG_PATH);
const LOG_LIMIT = 50;
const SPAM_INTERVAL_MS = 15000;

let lastNotificationTime = 0;
let lastNotificationMessage = '';

export const logNotification = async (type: string, message: string, icon: any, color: string) => {
    const currentTime = Date.now();
    const standardizedMessage = message.trim().toLowerCase();
    // Logic Chống Spam
    // if (
    //     currentTime - lastNotificationTime < SPAM_INTERVAL_MS &&
    //     standardizedMessage === lastNotificationMessage
    // ) {
    //     console.log(`[SPAM BLOCKED] Thông báo '${type}' bị chặn vì lặp lại trong vòng ${SPAM_INTERVAL_MS / 1000} giây.`);
    //     return;
    // }

    const newLogEntry = {
        type: type,
        message: message,
        icon: icon,
        color: color,
        timestamp: currentTime,
    };

    try {
        const newLogRef = push(notificationRef);
        await set(newLogRef, newLogEntry);

        lastNotificationTime = currentTime;
        lastNotificationMessage = standardizedMessage;

    } catch (error) {
        console.error("Lỗi khi ghi thông báo lên Firebase: ", error);
    }
};

// -------------------------------------------------------------------
// Component hiển thị từng thông báo
// -------------------------------------------------------------------
interface NotificationItemProps {
    type: string;
    message: string;
    timestamp: number | string;
    icon: any;
    color: string;
}

const formatTimestamp = (timestamp: number | string) => {
    if (typeof timestamp === 'number') {
        const date = new Date(timestamp);
        // Định dạng: HH:MM AM/PM - DD/MM/YYYY
        return date.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit' })
            + ' - ' + date.toLocaleDateString('vi-VN');
    }
    return timestamp;
};

const NotificationItem = ({ type, message, timestamp, icon, color }: NotificationItemProps) => (
    <View style={[styles.notificationItem, { borderLeftColor: color }]}>
        <Ionicons name={icon} size={28} color={color} style={styles.icon} />
        <View style={styles.notificationContent}>
            <View style={{ flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center' }}>
                <Text style={[styles.notificationType, { color: color }]}>{type}</Text>
                <Text style={styles.timestamp}>{formatTimestamp(timestamp)}</Text>
            </View>
            <Text style={styles.notificationMessage}>{message}</Text>
        </View>
    </View>
);

// -------------------------------------------------------------------
// Màn hình NotificationScreen
// -------------------------------------------------------------------
const NotificationScreen = () => {
    const router = useRouter();
    const [notifications, setNotifications] = useState<any[]>([]);
    const [isLoading, setIsLoading] = useState(true);

    const loadNotificationsFromFirebase = () => {
        setIsLoading(true);
        const logsQuery = query(notificationRef, orderByChild('timestamp'), limitToLast(LOG_LIMIT));

        const unsubscribe = onValue(logsQuery, (snapshot) => {
            const data = snapshot.val();
            let loadedNotifications: any[] = [];
            if (data) {
                loadedNotifications = Object.keys(data).map(key => ({
                    id: key,
                    ...data[key]
                }));
                loadedNotifications.sort((a, b) => b.timestamp - a.timestamp);
            }
            setNotifications(loadedNotifications);
            setIsLoading(false);
        }, (error) => {
            console.error("Lỗi khi tải thông báo từ Firebase:", error);
            Alert.alert("Lỗi", "Không thể tải lịch sử thông báo từ Firebase.");
            setIsLoading(false);
        });

        return unsubscribe;
    };

    // Hàm xóa toàn bộ log
    const clearLogs = async () => {
        try {
            await set(notificationRef, null); // Xóa toàn bộ node notifications
            setNotifications([]);
            Alert.alert("Hoàn tất", "Đã xóa toàn bộ lịch sử thông báo trên Firebase.");
        } catch (error) {
            console.error("Lỗi khi xóa log Firebase:", error);
            Alert.alert("Lỗi", "Không thể xóa lịch sử thông báo trên Firebase.");
        }
    };

    const handleClearLogs = () => {
        Alert.alert(
            "Xóa lịch sử",
            "Bạn có chắc chắn muốn xóa toàn bộ lịch sử thông báo trên Firebase?",
            [
                { text: "Hủy", style: "cancel" },
                { text: "Xóa", style: "destructive", onPress: clearLogs },
            ]
        );
    };

    useEffect(() => {
        const unsubscribe = loadNotificationsFromFirebase();
        return () => {
            unsubscribe();
        };
    }, []);

    const onViewHistory = () => {
        Alert.alert("Lịch sử thông báo", `Đang hiển thị ${notifications.length} thông báo gần nhất từ Firebase.`);
    };

    return (
        <ImageBackground
            source={backgroundImg}
            style={styles.backgroundImage}
            imageStyle={{ opacity: 0.5 }}
        >
            <View style={styles.fullContainer}>

                <View style={styles.mainContentArea}>
                    <Text style={styles.title}>THÔNG BÁO</Text>

                    <View style={styles.headerControls}>
                        <TouchableOpacity onPress={loadNotificationsFromFirebase} style={styles.controlButton}>
                            <Ionicons name="refresh-circle-outline" size={20} color="#0056b3" />
                            <Text style={[styles.controlButtonText, { color: '#0056b3' }]}>Tải lại</Text>
                        </TouchableOpacity>

                        <TouchableOpacity onPress={handleClearLogs} style={[styles.controlButton, { marginLeft: 15 }]}>
                            <Ionicons name="trash-outline" size={20} color="#d9534f" />
                            <Text style={[styles.controlButtonText, { color: '#d9534f' }]}>Xóa Log</Text>
                        </TouchableOpacity>

                        <TouchableOpacity onPress={onViewHistory} style={styles.historyButton}>
                            <Ionicons name="archive-outline" size={20} color="#11ad45bc" />
                            <Text style={styles.historyButtonText}>({notifications.length})</Text>
                        </TouchableOpacity>
                    </View>

                    <View style={styles.notificationScrollContainer}>
                        {isLoading ? (
                            <ActivityIndicator size="large" color="#11ad45bc" style={{ marginTop: 20 }} />
                        ) : notifications.length === 0 ? (
                            <Text style={styles.noNotificationText}>Không có thông báo nào được ghi nhận.</Text>
                        ) : (
                            <ScrollView>
                                {notifications.map((notif) => (
                                    <NotificationItem
                                        key={notif.id}
                                        type={notif.type}
                                        message={notif.message}
                                        timestamp={notif.timestamp}
                                        icon={notif.icon}
                                        color={notif.color}
                                    />
                                ))}
                            </ScrollView>
                        )}
                    </View>
                </View>
            </View>
        </ImageBackground>
    )
}

export default NotificationScreen;

const styles = StyleSheet.create({
    backgroundImage: {
        flex: 1,
    },
    fullContainer: {
        flex: 1,
    },
    mainContentArea: {
        flex: 1,
        paddingHorizontal: 15,
        paddingTop: 10,
    },
    title: {
        fontSize: 32,
        fontWeight: "bold",
        textAlign: 'center',
        color: '#11ad45bc',
        marginBottom: 15,
    },
    headerControls: {
        flexDirection: 'row',
        justifyContent: 'flex-end',
        alignItems: 'center',
        marginBottom: 10,
        marginRight: 5,
    },
    controlButton: {
        flexDirection: 'row',
        alignItems: 'center',
        paddingVertical: 5,
    },
    controlButtonText: {
        marginLeft: 5,
        fontWeight: 'bold',
        fontSize: 14,
    },
    historyButton: {
        flexDirection: 'row',
        alignItems: 'center',
        padding: 5,
        marginLeft: 20,
    },
    historyButtonText: {
        color: '#11ad45bc',
        marginLeft: 5,
        fontWeight: 'bold',
        fontSize: 14,
    },
    notificationScrollContainer: {
        flex: 1,
        backgroundColor: 'rgba(255, 255, 255, 0.9)',
        borderRadius: 20,
        padding: 10,
        borderWidth: 1,
        borderColor: '#eee',
        shadowColor: '#000',
        shadowOffset: { width: 0, height: 2 },
        shadowOpacity: 0.1,
        shadowRadius: 3,
        elevation: 3,
    },
    notificationItem: {
        flexDirection: 'row',
        backgroundColor: '#fff',
        padding: 15,
        borderRadius: 10,
        marginBottom: 10,
        borderLeftWidth: 5,
        alignItems: 'center',
        shadowColor: '#000',
        shadowOffset: { width: 0, height: 1 },
        shadowOpacity: 0.05,
        shadowRadius: 1,
        elevation: 1,
    },
    icon: {
        marginRight: 10,
    },
    notificationContent: {
        flex: 1,
    },
    notificationType: {
        fontWeight: 'bold',
        fontSize: 14,
        textTransform: 'uppercase',
    },
    notificationMessage: {
        fontSize: 16,
        color: '#333',
        marginTop: 2,
    },
    timestamp: {
        fontSize: 12,
        color: '#999',
    },
    noNotificationText: {
        textAlign: 'center',
        marginTop: 50,
        fontSize: 16,
        color: '#666',
        fontStyle: 'italic',
    }
});
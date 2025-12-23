import * as FileSystem from 'expo-file-system/legacy';
import { useRouter } from 'expo-router';
import { onValue, ref, set } from 'firebase/database';
import { Ionicons } from '@expo/vector-icons';
import React, { useEffect, useRef, useState } from 'react';
import { Alert, ImageBackground, ScrollView, StyleSheet, Text, TouchableOpacity, View } from 'react-native';
import { database } from 'E:/HTR/Project_6/Software/Smart-Farming-IoT/firebaseConfig.js';

const LOG_FILE_URI = FileSystem.documentDirectory + 'notification_log.json';

const FIREBASE_DATA_PATH = 'smart_farm_iot/user_data/user_1/data';
const FIREBASE_DATA_NODE1_PATH = `${FIREBASE_DATA_PATH}/node1`;
const FIREBASE_DATA_NODE2_PATH = `${FIREBASE_DATA_PATH}/node2`;
const FIREBASE_DATA_NODE3_PATH = `${FIREBASE_DATA_PATH}/node3`;
const CONFIG_PATH = 'smart_farm_iot/user_data/user_1/config/node_thresholds';
const NAME_CONFIG_PATH = 'smart_farm_iot/user_data/user_1/config/custom_names';

const RELAY_STATUS_ROOT = `${FIREBASE_DATA_PATH}`;
const RELAY1_CONTROL_PATH = `${RELAY_STATUS_ROOT}/relay1_status`;
const RELAY2_CONTROL_PATH = `${RELAY_STATUS_ROOT}/relay2_status`;
const RELAY3_CONTROL_PATH = `${RELAY_STATUS_ROOT}/relay3_status`;
const RELAY4_CONTROL_PATH = `${RELAY_STATUS_ROOT}/relay4_status`;

const dataNode1Ref = ref(database, FIREBASE_DATA_NODE1_PATH);
const dataNode2Ref = ref(database, FIREBASE_DATA_NODE2_PATH);
const dataNode3Ref = ref(database, FIREBASE_DATA_NODE3_PATH);
const relayStatusRef = ref(database, RELAY_STATUS_ROOT);
const nameConfigRef = ref(database, NAME_CONFIG_PATH);
const configRef = ref(database, CONFIG_PATH);

const backgroundImg = require('../../assets/backgrounds1.png');

// CẬP NHẬT TÊN MẶC ĐỊNH
const DEFAULT_NAMES = {
    node1: 'Sensor Node',
    node2: 'HMI Node',
    node3: 'Motivation Node',
    relay1: 'RL1 Pump',
    relay2: 'RL2 Light',
    relay3: 'RL3 Fan',
    relay4: 'RL4 Auto',
};

const logNotification = async (type: string, message: string, icon: any, color: string) => {
    const newLogEntry = {
        id: Date.now().toString(),
        type: type,
        message: message,
        icon: icon,
        color: color,
        timestamp: Date.now(),
    };

    try {
        const fileInfo = await FileSystem.getInfoAsync(LOG_FILE_URI);
        if (!fileInfo.exists) {
            await FileSystem.writeAsStringAsync(LOG_FILE_URI, '[]', { encoding: FileSystem.EncodingType.UTF8 });
        }
        let content = await FileSystem.readAsStringAsync(LOG_FILE_URI, { encoding: FileSystem.EncodingType.UTF8 });
        let logs: any[] = [];
        if (content) {
            try {
                logs = JSON.parse(content);
            } catch (e) {
                console.warn("Lỗi phân tích cú pháp file log, tạo lại file mới.");
                logs = [];
            }
        }
        logs.push(newLogEntry);
        logs.sort((a, b) => b.timestamp - a.timestamp);
        logs = logs.slice(0, 50);

        await FileSystem.writeAsStringAsync(
            LOG_FILE_URI,
            JSON.stringify(logs, null, 2),
            { encoding: FileSystem.EncodingType.UTF8 }
        );

    } catch (error) {
        console.error("Lỗi khi ghi thông báo vào file: ", error);
    }
};

const INITIAL_CARDS_CONFIG = [
    {
        id: 'node1',
        type: 'sensor',
        firebasePath: FIREBASE_DATA_NODE1_PATH,
        nameKey: 'node1',
        onlineKey: 'node1_state',
        resetFunc: () => ({
            mcu_temp: 0, dht_temp: 0, dht_humid: 0,
            light: 0, soil: 0, rain: 0
        }),
    },
    {
        id: 'node2',
        type: 'hmi',
        firebasePath: FIREBASE_DATA_NODE2_PATH,
        nameKey: 'node2',
        onlineKey: 'node2_state',
        resetFunc: () => ({ mcu_temp: 0, ssid: 'N/A' }),
    },
    {
        id: 'node3',
        type: 'motivation',
        firebasePath: FIREBASE_DATA_NODE3_PATH,
        nameKey: 'node3',
        onlineKey: 'node3_state',
        resetFunc: () => ({
            mcu_temp: 0, env_temp: 0, env_humid: 0,
            flow: 0
        }),
    },
];

const INITIAL_ALL_NODE_DATA = INITIAL_CARDS_CONFIG.reduce((acc, config) => {
    return { ...acc, [config.id]: config.resetFunc() };
}, {});

interface SensorCardProps {
    cardId: string;
    type: 'sensor' | 'hmi' | 'motivation';
    data: any;
    relayStates: { [key: string]: boolean };
    customNames: typeof DEFAULT_NAMES;
    currentConnection: boolean;
    toggleRelay: (currentState: boolean, path: string, relayName: string) => () => Promise<void>;
}

const SensorCard: React.FC<SensorCardProps> = ({
    cardId,
    type,
    data,
    relayStates,
    customNames,
    currentConnection,
    toggleRelay
}) => {
    const safeData = data || {};

    const renderContent = () => {
        switch (type) {
            case 'sensor':
                return (
                    <View style={styles.dataContainer}>
                        <Text style={styles.dataText}><Ionicons name="hardware-chip-outline" size={16} color="#333" /> MCU: {safeData.mcu_temp || 0} °C</Text>
                        <Text style={styles.dataText}><Ionicons name="thermometer-outline" size={16} color="#333" /> Nhiệt độ: {safeData.dht_temp || 0} °C</Text>
                        <Text style={styles.dataText}><Ionicons name="water-outline" size={16} color="#333" /> Độ ẩm KK: {safeData.dht_humid || 0} %</Text>
                        <Text style={styles.dataText}><Ionicons name="sunny-outline" size={16} color="#333" /> Cường độ sáng: {safeData.light || 0} %</Text>
                        <Text style={styles.dataText}><Ionicons name="leaf-outline" size={16} color="#333" /> Độ ẩm đất: {safeData.soil || 0} %</Text>
                        <Text style={styles.dataText}><Ionicons name="rainy-outline" size={16} color="#333" /> Trạng thái mưa: {safeData.rain == 1 ? " Raining" : " No Rain"}</Text>
                    </View>
                );
            case 'hmi':
                return (
                    <View style={styles.dataContainer}>
                        <Text style={styles.dataText}><Ionicons name="hardware-chip-outline" size={16} color="#333" /> MCU: {safeData.mcu_temp || 0} °C</Text>
                        <Text style={styles.dataText}><Ionicons name="wifi-outline" size={16} color="#333" /> SSID: {safeData.ssid || 'N/A'}</Text>
                    </View>
                );
            case 'motivation':
                return (
                    <View style={styles.dataContainer}>
                        <Text style={styles.dataText}><Ionicons name="hardware-chip-outline" size={16} color="#333" /> MCU: {safeData.mcu_temp || 0} °C</Text>
                        <Text style={styles.dataText}><Ionicons name="thermometer-outline" size={16} color="#333" /> Nhiệt độ: {safeData.env_temp || 0} °C</Text>
                        <Text style={styles.dataText}><Ionicons name="water-outline" size={16} color="#333" /> Độ ẩm KK: {safeData.env_humid || 0} %</Text>
                        <Text style={styles.dataText}><Ionicons name="speedometer-outline" size={16} color="#333" /> Lưu lượng: {safeData.flow || 0} cm³/s</Text>

                        <View style={styles.relayRow}>
                            {['relay1', 'relay2', 'relay3', 'relay4'].map((relayKey, index) => {
                                const paths = [RELAY1_CONTROL_PATH, RELAY2_CONTROL_PATH, RELAY3_CONTROL_PATH, RELAY4_CONTROL_PATH];
                                const path = paths[index];
                                const name = customNames[relayKey as keyof typeof DEFAULT_NAMES];
                                const isRelayOn = relayStates[relayKey];

                                const handleToggle = toggleRelay(isRelayOn, path, name);

                                return (
                                    <TouchableOpacity
                                        key={relayKey}
                                        style={[
                                            styles.button,
                                            { backgroundColor: isRelayOn ? "#11ad45" : "#5db2f8ff" }
                                        ]}
                                        onPress={handleToggle}
                                    >
                                        <Text style={styles.buttonText}>{name}</Text>
                                    </TouchableOpacity>
                                );
                            })}
                        </View>
                    </View>
                );
            default:
                return <Text style={styles.dataText}>Chưa hỗ trợ loại card này.</Text>;
        }
    };

    return (
        <View style={[styles.cardContainer, { width: "90%", marginBottom: 20 }]}>
            <View style={[
                styles.titleContainer,
                { backgroundColor: currentConnection ? "#5db2f8ff" : "#f85d5fff" }
            ]}>
                <Text style={styles.titleText}>{customNames[cardId as keyof typeof DEFAULT_NAMES] || cardId}</Text>
                <Text style={styles.titleText}>{currentConnection ? "ONLINE" : "OFFLINE"}</Text>
            </View>

            {renderContent()}
        </View>
    );
};


const AboutScreen = () => {
    const router = useRouter();

    const [allNodeData, setAllNodeData] = useState(INITIAL_ALL_NODE_DATA);
    const [connectionStatus, setConnectionStatus] = useState({ node1: false, node2: false, node3: false });
    const [relayStates, setRelayStates] = useState({
        relay1: false, relay2: false, relay3: false, relay4: false
    });
    const [customNames, setCustomNames] = useState(DEFAULT_NAMES);
    const [thresholds, setThresholds] = useState({ node1: 35, node3: 45 });
    const [isRainAlertEnabled, setIsRainAlertEnabled] = useState(true);

    const lastRainStatusRef = useRef(0);
    const lastDataTimeRefs = useRef<{ [key: string]: number }>({ node1: Date.now(), node2: Date.now(), node3: Date.now() });
    const lastDataValuesRefs = useRef<{ [key: string]: any }>({ node1: {}, node2: {}, node3: {} });

    const [cardConfigs, setCardConfigs] = useState(INITIAL_CARDS_CONFIG);

    const resetNodeData = (nodeId: string) => {
        const config = cardConfigs.find(c => c.id === nodeId);
        if (config) {
            setAllNodeData(prev => ({
                ...prev,
                [nodeId]: config.resetFunc()
            }));
        }
    };

    const hasDataChanged = (nodeId: string, newData: any, oldData: any) => {
        if (nodeId === 'node2') {
            const keys = ['node2_mcu_temp', 'node2_ssid'];
            return keys.some(key => newData[key] !== oldData[key]);
        }
        if (nodeId === 'node1') {
            const keys = ['node1_mcu_temp', 'node1_dht_temp', 'node1_dht_humid', 'node1_light', 'node1_soil', 'node1_rain'];
            return keys.some(key => newData[key] !== oldData[key]);
        }
        if (nodeId === 'node3') {
            const keys = ['node3_mcu_temp', 'node3_dht_temp', 'node3_dht_humid', 'node3_flow_rate'];
            return keys.some(key => newData[key] !== oldData[key]);
        }
        return false;
    };

    const toggleRelay = (currentState: boolean, path: string, relayName: string) => async () => {
        const newStateValue = currentState ? false : true;
        const actionText = newStateValue === true ? 'BẬT' : 'TẮT';
        const relayKey = Object.keys(customNames).find(key => customNames[key as keyof typeof DEFAULT_NAMES] === relayName);

        try {
            const relayControlRef = ref(database, path);
            await set(relayControlRef, newStateValue);

            if (relayKey) {
                setRelayStates(prev => ({ ...prev, [relayKey]: newStateValue }));
            }

            await logNotification(
                'ĐIỀU KHIỂN',
                `Đã ${actionText} ${relayName} thành công.`,
                'settings',
                '#11ad45'
            );
        } catch (error) {
            console.error(`Lỗi khi cập nhật ${relayName}: `, error);
            Alert.alert("Lỗi", `Không thể cập nhật ${relayName} trên Firebase.`);
        }
    };

    useEffect(() => {
        const checkThreshold = async (temp: number, threshold: number, nodeName: string, tempType: string) => {
            if (temp > threshold) {
                await logNotification(
                    'CẢNH BÁO',
                    `${nodeName}: ${tempType} (${temp}°C) vượt ngưỡng an toàn (${threshold}°C).`,
                    'warning',
                    '#d9534f'
                );
            }
        };

        const unsubscribeNames = onValue(nameConfigRef, (snapshot) => {
            if (snapshot.exists()) {
                const loadedNames = snapshot.val();
                setCustomNames({ ...DEFAULT_NAMES, ...loadedNames });
            } else {
                setCustomNames(DEFAULT_NAMES);
            }
        }, (error) => {
            console.error("Lỗi khi tải tên tùy chỉnh:", error);
        });

        const unsubscribeConfig = onValue(configRef, (snapshot) => {
            if (snapshot.exists()) {
                const config = snapshot.val();
                setThresholds(config.thresholds || { node1: 35, node3: 45 });
                setIsRainAlertEnabled(config.isRainAlertEnabled !== undefined ? config.isRainAlertEnabled : true);
            }
        }, (error) => {
            console.error("Lỗi khi tải cấu hình Node:", error);
        });

        const unsubscribers: (() => void)[] = [];

        cardConfigs.forEach(config => {
            const nodeRef = ref(database, config.firebasePath);
            const unsubscribe = onValue(nodeRef, (snapshot) => {
                const data = snapshot.val();
                const nodeId = config.id;

                if (data) {
                    if (hasDataChanged(nodeId, data, lastDataValuesRefs.current[nodeId])) {
                        lastDataTimeRefs.current = { ...lastDataTimeRefs.current, [nodeId]: Date.now() };
                    }

                    let newInnerData: any = {};

                    if (nodeId === 'node1') {
                        newInnerData = {
                            mcu_temp: data.node1_mcu_temp || 0,
                            dht_temp: data.node1_dht_temp || 0,
                            dht_humid: data.node1_dht_humid || 0,
                            light: data.node1_light || 0,
                            soil: data.node1_soil || 0,
                            rain: data.node1_rain || 0,
                        };
                        const nodeName = customNames.node1;
                        if (data[config.onlineKey] === true || data[config.onlineKey] === 1) {
                            checkThreshold(newInnerData.dht_temp, thresholds.node1, nodeName, 'Nhiệt độ môi trường');
                            checkThreshold(newInnerData.mcu_temp, thresholds.node1 + 5, nodeName, 'Nhiệt độ MCU');
                        }

                        const isRainingNow = newInnerData.rain == 1;
                        const wasRainingBefore = lastRainStatusRef.current == 1;
                        if (isRainAlertEnabled && isRainingNow && !wasRainingBefore) {
                            logNotification(
                                'CẢNH BÁO', 'Trạng thái MƯA đã BẬT. Cần kiểm tra hệ thống che chắn.',
                                'cloud-drizzle', '#f0ad4e'
                            );
                        }
                        lastRainStatusRef.current = newInnerData.rain || 0;

                    } else if (nodeId === 'node2') {
                        newInnerData = {
                            mcu_temp: data.node2_mcu_temp || 0,
                            ssid: data.node2_ssid || 'N/A',
                        };
                    }
                    else if (nodeId === 'node3') {
                        newInnerData = {
                            mcu_temp: data.node3_mcu_temp || 0,
                            env_temp: data.node3_dht_temp || 0,
                            env_humid: data.node3_dht_humid || 0,
                            flow: data.node3_flow_rate || 0,
                        };
                        const nodeName = customNames.node3;
                        if (data[config.onlineKey] === true || data[config.onlineKey] === 1) {
                            checkThreshold(newInnerData.env_temp, thresholds.node3, nodeName, 'Nhiệt độ môi trường');
                            checkThreshold(newInnerData.mcu_temp, thresholds.node3 + 5, nodeName, 'Nhiệt độ MCU');
                        }
                    }

                    setAllNodeData(prev => ({
                        ...prev,
                        [nodeId]: newInnerData
                    }));

                    const isOnline = data[config.onlineKey] === true || data[config.onlineKey] === 1;
                    setConnectionStatus(prev => ({ ...prev, [nodeId]: isOnline }));

                    lastDataValuesRefs.current = { ...lastDataValuesRefs.current, [nodeId]: data };

                } else {
                    setConnectionStatus(prev => ({ ...prev, [nodeId]: false }));
                    resetNodeData(nodeId);
                }
            }, (error) => {
                console.error(`Lỗi khi đọc dữ liệu Node Firebase: `, error);
            });
            unsubscribers.push(unsubscribe);
        });

        const unsubscribeRelayStatus = onValue(relayStatusRef, (snapshot) => {
            const data = snapshot.val();
            if (data) {
                setRelayStates({
                    relay1: data.relay1_status === true || data.relay1_status === 1,
                    relay2: data.relay2_status === true || data.relay2_status === 1,
                    relay3: data.relay3_status === true || data.relay3_status === 1,
                    relay4: data.relay4_status === true || data.relay4_status === 1,
                });
            }
        });

        const CONNECTION_TIMEOUT = 10000;
        const CHECK_INTERVAL = 2000;
        const intervalId = setInterval(() => {
            cardConfigs.forEach(config => {
                const nodeId = config.id;
                const timeSinceLastChange = Date.now() - lastDataTimeRefs.current[nodeId];

                if (timeSinceLastChange > CONNECTION_TIMEOUT) {
                    if (connectionStatus[nodeId as keyof typeof connectionStatus]) {
                        setConnectionStatus(prev => ({ ...prev, [nodeId]: false }));
                        resetNodeData(nodeId);
                        logNotification(
                            'HỆ THỐNG',
                            `MẤT KẾT NỐI: ${customNames[config.nameKey as keyof typeof DEFAULT_NAMES]} đã ngắt kết nối với hệ thống.`,
                            'close-circle',
                            '#d9534f'
                        );
                    }
                } else if (timeSinceLastChange <= CONNECTION_TIMEOUT && !connectionStatus[nodeId as keyof typeof connectionStatus]) {
                    setConnectionStatus(prev => ({ ...prev, [nodeId]: true }));
                }
            });
        }, CHECK_INTERVAL);

        return () => {
            unsubscribeNames();
            unsubscribeConfig();
            unsubscribers.forEach(unsub => unsub());
            unsubscribeRelayStatus();
            clearInterval(intervalId);
        };
    }, [customNames, thresholds, isRainAlertEnabled, cardConfigs]);

    const boundToggleRelay = (isRelayOn: boolean, path: string, relayName: string) => toggleRelay(isRelayOn, path, relayName);

    return (
        <ImageBackground source={backgroundImg} style={styles.backgroundImage}>
            <ScrollView contentContainerStyle={styles.containerscroll}>
                <View style={styles.mainContent}>
                    <Text style={styles.sectionTitle}>Smart Farm</Text>

                    {cardConfigs.map(config => (
                        <SensorCard
                            key={config.id}
                            cardId={config.id}
                            type={config.type as any}
                            data={allNodeData[config.id as keyof typeof allNodeData]}
                            relayStates={relayStates}
                            customNames={customNames}
                            currentConnection={connectionStatus[config.id as keyof typeof connectionStatus]}
                            toggleRelay={boundToggleRelay}
                        />
                    ))}

                </View>
            </ScrollView>
        </ImageBackground>
    )
}

export default AboutScreen

const styles = StyleSheet.create({
    backgroundImage: {
        flex: 1,
    },
    containerscroll: {
        flexGrow: 1,
        alignItems: "center",
        paddingVertical: 20,
    },
    mainContent: {
        width: "100%",
        alignItems: "center",
    },
    sectionTitle: {
        fontSize: 28,
        fontWeight: "bold",
        color: '#333',
        marginBottom: 10,
        alignSelf: 'flex-start',
        marginLeft: '5%',
    },

    cardContainer: {
        backgroundColor: "#fff",
        opacity: 0.9,
        borderRadius: 20,
        shadowColor: "#000",
        shadowOffset: { width: 0, height: 2 },
        shadowOpacity: 0.25,
        shadowRadius: 3.84,
        elevation: 5,
        alignItems: "center",
        paddingBottom: 15,
        minHeight: 250,
    },
    titleContainer: {
        width: "100%",
        height: 50,
        borderTopLeftRadius: 20,
        borderTopRightRadius: 20,
        flexDirection: 'row',
        justifyContent: "space-between",
        alignItems: "center",
        paddingHorizontal: 15,
        marginBottom: 10,
    },
    titleText: {
        fontSize: 18,
        fontWeight: "bold",
        color: "#fff"
    },
    dataContainer: {
        width: "90%",
        paddingHorizontal: 10,
    },
    dataText: {
        fontSize: 16,
        color: "#5a5858",
        marginBottom: 5
    },

    relayRow: {
        flexDirection: "row",
        justifyContent: "space-between",
        marginTop: 15,
        width: "100%",
    },
    button: {
        width: "22%",
        height: 40,
        borderRadius: 10,
        justifyContent: "center",
        alignItems: "center",
    },
    buttonText: {
        fontWeight: "bold",
        color: '#fff',
        fontSize: 12,
    },
})
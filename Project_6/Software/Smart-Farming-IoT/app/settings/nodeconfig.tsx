import { Ionicons } from '@expo/vector-icons';
import DateTimePicker from '@react-native-community/datetimepicker';
import { useRouter } from 'expo-router';
import { onValue, ref, set } from 'firebase/database';
import React, { useEffect, useState, useRef } from 'react';
import { Alert, KeyboardAvoidingView, Modal, Platform, Pressable, ScrollView, StyleSheet, Switch, Text, TextInput, TouchableOpacity, View } from 'react-native';
import { database } from 'E:/HTR/Project_6/Software/Smart-Farming-IoT/firebaseConfig.js';

const USER_ID = 'user_1';
const FIREBASE_CONFIG_ROOT = `smart_farm_iot/user_data/${USER_ID}/config`;

const TIMER_PATH = `${FIREBASE_CONFIG_ROOT}/timers`;
const THRESHOLDS_PATH = `${FIREBASE_CONFIG_ROOT}/node_thresholds/thresholds`;
const RAIN_ALERT_PATH = `${FIREBASE_CONFIG_ROOT}/node_thresholds/isRainAlertEnabled`;
const NAME_CONFIG_PATH = `smart_farm_iot/user_data/${USER_ID}/config/custom_names`;

const RELAY_CONTROL_ROOT = `smart_farm_iot/user_data/${USER_ID}/data`;

const timerRef = ref(database, TIMER_PATH);
const thresholdsRef = ref(database, THRESHOLDS_PATH);
const rainAlertRef = ref(database, RAIN_ALERT_PATH);
const nameConfigRef = ref(database, NAME_CONFIG_PATH);

const logNotification = (type: string, message: string, icon: any, color: string) => {
  console.log(`[LOGGING SIMULATED] Type: ${type}, Message: ${message}, Icon: ${icon}, Color: ${color}`);
};

type RelayKey = 'RL1' | 'RL2' | 'RL3' | 'RL4';
const DEFAULT_RELAYS: RelayKey[] = ['RL1', 'RL2', 'RL3', 'RL4'];

const getRelayPath = (relayKey: RelayKey): string => {
  switch (relayKey) {
    case 'RL1': return `${RELAY_CONTROL_ROOT}/relay1_status`;
    case 'RL2': return `${RELAY_CONTROL_ROOT}/relay2_status`;
    case 'RL3': return `${RELAY_CONTROL_ROOT}/relay3_status`;
    case 'RL4': return `${RELAY_CONTROL_ROOT}/relay4_status`;
    default: return '';
  }
};

interface Timer {
  id: number;
  name: string;
  relayKey: RelayKey;
  relayName: string;
  time: string; // "HH:MM"
  duration: number; // giây
  enabled: boolean;
}

interface Thresholds {
  node1: number;
  node2: number;
  node3: number;
  envNode1: number;
  envNode3: number;
}

const SettingSection = ({ title, children }: { title: string; children: React.ReactNode }) => (
  <View style={styles.settingSection}>
    <Text style={styles.sectionTitle}>{title}</Text>
    <View style={styles.sectionContent}>
      {children}
    </View>
  </View>
);

const TimerEditModal = ({ isVisible, timer, customRelayNames, onClose, onSave }: { isVisible: boolean, timer: Timer, customRelayNames: { [key in RelayKey]?: string }, onClose: () => void, onSave: (timer: Timer) => void }) => {
  const [tempTimer, setTempTimer] = useState(timer);
  const [showTimePicker, setShowTimePicker] = useState(false);

  useEffect(() => {
    setTempTimer(timer);
  }, [timer]);

  const handleTimeChange = (event: any, selectedDate: Date | undefined) => {
    setShowTimePicker(false);
    if (selectedDate) {
      const hours = selectedDate.getHours().toString().padStart(2, '0');
      const minutes = selectedDate.getMinutes().toString().padStart(2, '0');
      setTempTimer(prev => ({ ...prev, time: `${hours}:${minutes}` }));
    }
  };

  const now = new Date();
  const [h, m] = tempTimer.time.split(':').map(Number);
  const selectedTime = new Date(now.getFullYear(), now.getMonth(), now.getDate(), h, m);

  const relayOptions: { key: RelayKey, name: string }[] = DEFAULT_RELAYS.map(key => ({
    key: key,
    name: customRelayNames[key] || key,
  }));

  const handleDurationChange = (text: string) => {
    const num = parseInt(text) || 0;
    setTempTimer(prev => ({ ...prev, duration: num > 0 ? num : 1 }));
  };

  return (
    <Modal
      animationType="slide"
      transparent={true}
      visible={isVisible}
      onRequestClose={onClose}
    >
      <KeyboardAvoidingView
        behavior={Platform.OS === "ios" ? "padding" : "height"}
        style={modalStyles.centeredView}
      >
        <ScrollView
          contentContainerStyle={modalStyles.scrollContent}
          keyboardShouldPersistTaps="handled"
        >
          <View style={modalStyles.modalView}>

            <Text style={modalStyles.modalTitle}>Chỉnh Sửa Timer</Text>

            <Text style={modalStyles.label}>Tên Timer:</Text>
            <TextInput
              style={modalStyles.input}
              value={tempTimer.name}
              onChangeText={(text) => setTempTimer(prev => ({ ...prev, name: text }))}
            />

            <Text style={modalStyles.label}>Đối tượng Tác động (Relay):</Text>
            <View style={modalStyles.relaySelection}>
              {relayOptions.map((r) => (
                <TouchableOpacity
                  key={r.key}
                  style={[
                    modalStyles.relayButton,
                    tempTimer.relayKey === r.key && modalStyles.relayButtonSelected
                  ]}
                  onPress={() => setTempTimer(prev => ({ ...prev, relayKey: r.key, relayName: r.name }))}
                >
                  <Text style={[
                    modalStyles.relayButtonText,
                    tempTimer.relayKey === r.key && modalStyles.relayButtonTextSelected
                  ]}>{r.name}</Text>
                </TouchableOpacity>
              ))}
            </View>

            <Text style={modalStyles.label}>Thời gian tác động:</Text>
            <TouchableOpacity onPress={() => setShowTimePicker(true)} style={modalStyles.timeInputButton}>
              <Text style={modalStyles.timeInputText}>{tempTimer.time}</Text>
              <Ionicons name="time-outline" size={24} color="#333" />
            </TouchableOpacity>
            {showTimePicker && (
              <DateTimePicker
                value={selectedTime}
                mode="time"
                display="default"
                is24Hour={true}
                onChange={handleTimeChange}
              />
            )}

            <Text style={modalStyles.label}>Thời lượng tác động (giây):</Text>
            <TextInput
              style={modalStyles.input}
              value={String(tempTimer.duration)}
              onChangeText={handleDurationChange}
              keyboardType="numeric"
            />

            <View style={modalStyles.buttonContainer}>
              <Pressable
                style={[modalStyles.button, modalStyles.buttonClose]}
                onPress={onClose}
              >
                <Text style={modalStyles.textStyle}>Hủy</Text>
              </Pressable>
              <Pressable
                style={[modalStyles.button, modalStyles.buttonSave]}
                onPress={() => onSave(tempTimer)}
              >
                <Text style={modalStyles.textStyle}>Lưu</Text>
              </Pressable>
            </View>
          </View>
        </ScrollView>
      </KeyboardAvoidingView>
    </Modal>
  );
};

const TimerItem = ({ timer, onToggle, onRemove, onEdit }: { timer: Timer, onToggle: (id: number, enabled: boolean) => void, onRemove: (id: number) => void, onEdit: (timer: Timer) => void }) => {
  const handleTimerLogic = (isEnabled: boolean) => {
    onToggle(timer.id, isEnabled);
    const action = isEnabled ? 'ĐÃ BẬT' : 'ĐÃ TẮT';
    const logColor = isEnabled ? '#5cb85c' : '#f0ad4e';

    logNotification(
      'TIMER',
      `Timer '${timer.name}' cho ${timer.relayName} ${action} thủ công. (Lịch: ${timer.time} trong ${timer.duration}s)`,
      'time-outline',
      logColor
    );
  };

  return (
    <TouchableOpacity style={styles.timerItem} onPress={() => onEdit(timer)}>
      <View style={styles.timerInfo}>
        <Text style={styles.timerRelay}>{timer.name} ({timer.relayName})</Text>
        <Text style={styles.timerDetails}>
          Kích hoạt lúc: {timer.time} | Kéo dài: {timer.duration} giây
        </Text>
      </View>
      <View style={styles.timerActions}>
        <Switch
          trackColor={{ false: "#767577", true: "#11ad45bc" }}
          thumbColor={timer.enabled ? "#f4f3f4" : "#f4f3f4"}
          onValueChange={handleTimerLogic}
          value={timer.enabled}
        />
        <TouchableOpacity onPress={() => onRemove(timer.id)} style={styles.removeButton}>
          <Ionicons name="trash-outline" size={24} color="#d9534f" />
        </TouchableOpacity>
      </View>
    </TouchableOpacity>
  );
}

const NodeConfigScreen = () => {
  const router = useRouter();

  const [timers, setTimers] = useState<Timer[]>([]);
  const [thresholds, setThresholds] = useState<Thresholds>({
    node1: 35,
    node2: 40,
    node3: 45,
    envNode1: 35,
    envNode3: 35,
  });
  const [isRainAlertEnabled, setIsRainAlertEnabled] = useState(false);
  const [customRelayNames, setCustomRelayNames] = useState<{ [key in RelayKey]?: string }>({});
  const activatedTimersRef = useRef<{ [key: number]: boolean }>({});

  const [isModalVisible, setIsModalVisible] = useState(false);
  const [editingTimer, setEditingTimer] = useState<Timer | null>(null);

  const getDisplayRelayName = (key: RelayKey) => customRelayNames[key] || key;

  const updateTimersOnFirebase = (newTimers: Timer[]) => {
    const timersObject = newTimers.reduce((obj, timer) => {
      const { relayName, ...firebaseData } = timer;
      obj[timer.id.toString()] = firebaseData;
      return obj;
    }, {} as { [key: string]: Omit<Timer, 'id' | 'relayName'> });

    set(timerRef, timersObject)
      .catch(error => Alert.alert("Lỗi", "Không thể lưu Timer lên Firebase."));
  };

  const activateRelay = async (timer: Timer) => {
    const relayPath = getRelayPath(timer.relayKey);

    if (!relayPath) {
      logNotification('LỖI', `Không tìm thấy đường dẫn Firebase cho Relay: ${timer.relayKey}`, 'warning', '#d9534f');
      return;
    }

    try {
      await set(ref(database, relayPath), true);
      logNotification(
        'TIMER START',
        `Timer '${timer.name}' ĐÃ BẬT ${timer.relayName}. Thời lượng: ${timer.duration}s.`,
        'play-circle',
        '#11ad45bc'
      );

      setTimeout(async () => {
        await set(ref(database, relayPath), false);
        logNotification(
          'TIMER END',
          `Timer '${timer.name}' ĐÃ TẮT ${timer.relayName} sau ${timer.duration}s.`,
          'stop-circle',
          '#f0ad4e'
        );
      }, timer.duration * 1000);

      activatedTimersRef.current = { ...activatedTimersRef.current, [timer.id]: true };
    } catch (error) {
      logNotification('LỖI', `Lỗi kích hoạt/tắt Relay ${timer.relayName}: ${error}`, 'bug', '#d9534f');
    }
  };

  useEffect(() => {
    const unsubscribeNames = onValue(nameConfigRef, (snapshot) => {
      const loadedNames = (snapshot.exists() ? snapshot.val() : {}) as { [key: string]: string };
      const mappedNames: { [key in RelayKey]?: string } = {};
      DEFAULT_RELAYS.forEach(key => {
        if (loadedNames[key.toLowerCase()]) {
          mappedNames[key] = loadedNames[key.toLowerCase()];
        }
      });
      setCustomRelayNames(mappedNames);
    }, (error) => {
      console.error("Lỗi khi tải tên tùy chỉnh:", error);
    });

    const unsubscribeTimer = onValue(timerRef, (snapshot) => {
      const data = snapshot.val();
      if (data) {
        const loadedTimers: Timer[] = Object.keys(data).map(key => {
          const timerData = data[key];
          const relayKey = timerData.relayKey as RelayKey;

          return {
            id: Number(key),
            ...timerData,
            relayKey: relayKey,
            relayName: getDisplayRelayName(relayKey),
            name: timerData.name || `Timer ${key}`,
          }
        });
        setTimers(loadedTimers.sort((a, b) => a.id - b.id));
      } else {
        setTimers([]);
      }
    }, (error) => {
      console.error("Lỗi khi tải Timer từ Firebase:", error);
    });
    const timerCheckInterval = setInterval(() => {
      const now = new Date();
      const currentHour = now.getHours().toString().padStart(2, '0');
      const currentMinute = now.getMinutes().toString().padStart(2, '0');
      const currentTimeStr = `${currentHour}:${currentMinute}`;

      if (currentTimeStr === '00:00') {
        activatedTimersRef.current = {};
        logNotification('HỆ THỐNG', 'Đã reset trạng thái kích hoạt Timer cho ngày mới.', 'calendar', '#6c757d');
      }

      timers.forEach(timer => {
        if (timer.enabled && timer.time === currentTimeStr && !activatedTimersRef.current[timer.id]) {
          activateRelay(timer);
        }
      });

    }, 10000);

    const unsubscribeThresholds = onValue(thresholdsRef, (snapshot) => {
      const data = snapshot.val();
      if (data) {
        setThresholds({
          node1: data.node1 || 35,
          node2: data.node2 || 40,
          node3: data.node3 || 45,
          envNode1: data.envNode1 || 35,
          envNode3: data.envNode3 || 35,
        });
      } else {
        setThresholds({
          node1: 35,
          node2: 40,
          node3: 45,
          envNode1: 35,
          envNode3: 35,
        });
      }
    }, (error) => {
      console.error("Lỗi khi tải Ngưỡng từ Firebase:", error);
    });

    const unsubscribeRainAlert = onValue(rainAlertRef, (snapshot) => {
      const data = snapshot.val();
      setIsRainAlertEnabled(data !== null ? data : true);
    }, (error) => {
      console.error("Lỗi khi tải Cảnh báo Mưa từ Firebase:", error);
    });

    return () => {
      clearInterval(timerCheckInterval);
      unsubscribeNames();
      unsubscribeTimer();
      unsubscribeThresholds();
      unsubscribeRainAlert();
    };
  }, [timers, customRelayNames]);

  const onBackToSetting = () => {
    router.replace("/setting");
  };

  const handleAddTimer = () => {
    const newId = Date.now();
    const defaultRelayKey: RelayKey = DEFAULT_RELAYS[0];

    const newTimer: Timer = {
      id: newId,
      name: `Timer mới ${newId.toString().slice(-4)}`,
      relayKey: defaultRelayKey,
      relayName: getDisplayRelayName(defaultRelayKey),
      time: '06:00',
      duration: 60,
      enabled: true,
    };
    const newTimers = [...timers, newTimer].sort((a, b) => a.id - b.id);
    setTimers(newTimers);
    updateTimersOnFirebase(newTimers);
    Alert.alert("Thành công", `Đã thêm Timer '${newTimer.name}'.`);
  };

  const handleEditTimer = (timer: Timer) => {
    setEditingTimer(timer);
    setIsModalVisible(true);
  };

  const handleSaveTimer = (updatedTimer: Timer) => {
    const finalTimer = {
      ...updatedTimer,
      relayName: getDisplayRelayName(updatedTimer.relayKey)
    };

    const newTimers = timers.map(t => t.id === finalTimer.id ? finalTimer : t);
    setTimers(newTimers);
    updateTimersOnFirebase(newTimers);
    setIsModalVisible(false);
    setEditingTimer(null);
  };

  const handleRemoveTimer = (id: number) => {
    Alert.alert(
      "Xác nhận xóa",
      "Bạn có chắc chắn muốn xóa Timer này?",
      [
        { text: "Hủy", style: "cancel" },
        {
          text: "Xóa", onPress: () => {
            const newTimers = timers.filter(t => t.id !== id);
            setTimers(newTimers);
            updateTimersOnFirebase(newTimers);
          }
        }
      ]
    );
  };

  const handleToggleTimer = (id: number, enabled: boolean) => {
    const newTimers = timers.map(t => t.id === id ? { ...t, enabled } : t);
    setTimers(newTimers);
    updateTimersOnFirebase(newTimers);
  };

  const handleThresholdChange = (node: keyof Thresholds, value: string) => {
    const numValue = parseInt(value);
    const newThresholds = { ...thresholds, [node]: (isNaN(numValue) || numValue < 0) ? 0 : numValue };
    setThresholds(newThresholds);
    set(thresholdsRef, newThresholds)
      .catch(error => Alert.alert("Lỗi", "Không thể lưu Ngưỡng lên Firebase."));
  };

  const handleRainAlertToggle = (isEnabled: boolean) => {
    setIsRainAlertEnabled(isEnabled);

    set(rainAlertRef, isEnabled)
      .catch(error => Alert.alert("Lỗi", "Không thể lưu trạng thái cảnh báo mưa lên Firebase."));

    const action = isEnabled ? 'ĐÃ BẬT' : 'ĐÃ TẮT';
    const logMessage = isEnabled
      ? 'Cảnh báo mưa đã được BẬT. Mọi thay đổi trạng thái mưa sẽ được ghi lại.'
      : 'Cảnh báo mưa đã được TẮT. Trạng thái mưa sẽ không được ghi vào Log.';

    logNotification(
      'HỆ THỐNG',
      logMessage,
      'settings-outline',
      '#11ad45bc'
    );
  };

  const renderThresholds = (keys: (keyof Thresholds)[], title: string) => (
    <View>
      <Text style={styles.subSectionTitle}>{title}</Text>
      {keys.map((node) => {
        let label = '';
        if (node === 'node1') label = 'Node 1 (Sensor) MCU:';
        else if (node === 'node2') label = 'Node 2 (HMI) MCU:';
        else if (node === 'node3') label = 'Node 3 (Motivation) MCU:';
        else if (node === 'envNode1') label = 'Node 1 Môi trường:';
        else if (node === 'envNode3') label = 'Node 3 Môi trường:';

        return (
          <View key={node} style={styles.thresholdRow}>
            <Text style={styles.thresholdLabel}>{label}</Text>
            <TextInput
              style={styles.thresholdInput}
              onChangeText={(text) => handleThresholdChange(node, text)}
              value={String(thresholds[node])}
              keyboardType="numeric"
              placeholder="Ngưỡng (°C)"
            />
          </View>
        );
      })}
    </View>
  );

  return (
    <View style={styles.container}>
      <View style={styles.header}>
        <TouchableOpacity onPress={onBackToSetting} style={styles.backButton}>
          <Ionicons name="arrow-back" size={24} color="#333" />
          <Text style={styles.headerText}>Cài đặt</Text>
        </TouchableOpacity>
      </View>
      <Text style={styles.title}>THIẾT LẬP NODE</Text>

      <ScrollView style={{ flex: 1 }}>

        <SettingSection title="1. Quản lý Timer Relay (4 Rơ-le)">
          {timers.map((timer) => (
            <TimerItem
              key={timer.id}
              timer={timer}
              onToggle={handleToggleTimer}
              onRemove={handleRemoveTimer}
              onEdit={handleEditTimer}
            />
          ))}
          <TouchableOpacity onPress={handleAddTimer} style={styles.addButton}>
            <Ionicons name="add-circle" size={28} color="#fff" />
            <Text style={styles.addButtonText}>Thêm Timer mới</Text>
          </TouchableOpacity>
        </SettingSection>

        <SettingSection title="2. Ngưỡng Cảnh Báo Nhiệt Độ (°C)">
          {renderThresholds(['node1', 'node2', 'node3'], 'Nhiệt độ MCU')}
          <View style={styles.separator} />
          {renderThresholds(['envNode1', 'envNode3'], 'Nhiệt độ Môi trường')}
        </SettingSection>

        <SettingSection title="3. Công Tắc Cảnh Báo Mưa">
          <View style={styles.rainAlertRow}>
            <Text style={styles.rainAlertLabel}>Cảnh báo mưa</Text>
            <Switch
              trackColor={{ false: "#767577", true: "#11ad45bc" }}
              thumbColor={isRainAlertEnabled ? "#f4f3f4" : "#f4f3f4"}
              onValueChange={handleRainAlertToggle}
              value={isRainAlertEnabled}
            />
          </View>
        </SettingSection>
        <View style={{ height: 50 }} />
      </ScrollView>

      {editingTimer && (
        <TimerEditModal
          isVisible={isModalVisible}
          timer={editingTimer}
          customRelayNames={customRelayNames}
          onClose={() => {
            setIsModalVisible(false);
            setEditingTimer(null);
          }}
          onSave={handleSaveTimer}
        />
      )}
    </View>
  );
};

export default NodeConfigScreen;

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#f5f5f5',
    padding: 15,
  },
  header: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingVertical: 10,
    marginBottom: 10,
  },
  backButton: {
    flexDirection: 'row',
    alignItems: 'center',
    padding: 5,
  },
  headerText: {
    fontSize: 18,
    fontWeight: 'bold',
    color: '#333',
    marginLeft: 5,
  },
  title: {
    fontSize: 24,
    fontWeight: 'bold',
    textAlign: 'center',
    marginVertical: 10,
    color: '#11ad45bc',
  },
  settingSection: {
    backgroundColor: '#fff',
    borderRadius: 10,
    padding: 15,
    marginBottom: 20,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 3,
    elevation: 3,
  },
  sectionTitle: {
    fontSize: 18,
    fontWeight: 'bold',
    color: '#333',
    marginBottom: 10,
    borderBottomWidth: 1,
    borderBottomColor: '#eee',
    paddingBottom: 5,
  },
  sectionContent: {
    paddingTop: 5,
  },
  subSectionTitle: {
    fontSize: 16,
    fontWeight: 'bold',
    color: '#11ad45bc',
    marginTop: 5,
    marginBottom: 10,
  },
  separator: {
    height: 1,
    backgroundColor: '#eee',
    marginVertical: 15,
  },
  timerItem: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingVertical: 10,
    borderBottomWidth: 1,
    borderBottomColor: '#f5f5f5',
  },
  timerInfo: {
    flex: 1,
  },
  timerRelay: {
    fontSize: 16,
    fontWeight: 'bold',
    color: '#11ad45bc',
  },
  timerDetails: {
    fontSize: 13,
    color: '#666',
  },
  timerActions: {
    flexDirection: 'row',
    alignItems: 'center',
  },
  removeButton: {
    marginLeft: 15,
    padding: 5,
  },
  addButton: {
    flexDirection: 'row',
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: '#11ad45bc',
    padding: 10,
    borderRadius: 8,
    marginTop: 10,
  },
  addButtonText: {
    color: '#fff',
    fontWeight: 'bold',
    fontSize: 16,
    marginLeft: 5,
  },
  thresholdRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 10,
  },
  thresholdLabel: {
    fontSize: 16,
    color: '#333',
    flex: 2,
  },
  thresholdInput: {
    flex: 1,
    borderWidth: 1,
    borderColor: '#ddd',
    borderRadius: 5,
    padding: 8,
    textAlign: 'center',
    fontSize: 16,
  },
  rainAlertRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 5,
  },
  rainAlertLabel: {
    fontSize: 16,
    color: '#333',
    fontWeight: '500',
  },
  rainAlertDescription: {
    fontSize: 12,
    color: '#777',
    marginTop: 5,
  }
});

const modalStyles = StyleSheet.create({
  centeredView: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: 'rgba(0, 0, 0, 0.5)',
  },
  modalView: {
    margin: 20,
    backgroundColor: 'white',
    borderRadius: 20,
    padding: 35,
    alignItems: 'center',
    shadowColor: '#000',
    shadowOffset: {
      width: 0,
      height: 2,
    },
    shadowOpacity: 0.25,
    shadowRadius: 4,
    elevation: 5,
    width: '90%',
  },
  scrollContent: {
    flexGrow: 1,
    justifyContent: 'center',
  },
  modalTitle: {
    fontSize: 22,
    fontWeight: 'bold',
    marginBottom: 20,
    color: '#11ad45bc',
  },
  label: {
    alignSelf: 'flex-start',
    fontSize: 16,
    marginTop: 10,
    marginBottom: 5,
    fontWeight: '600',
  },
  input: {
    width: '100%',
    borderWidth: 1,
    borderColor: '#ccc',
    borderRadius: 10,
    padding: 10,
    marginBottom: 10,
  },
  timeInputButton: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    width: '100%',
    borderWidth: 1,
    borderColor: '#ccc',
    borderRadius: 10,
    padding: 10,
    marginBottom: 10,
  },
  timeInputText: {
    fontSize: 18,
    color: '#333',
  },
  relaySelection: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    justifyContent: 'space-between',
    width: '100%',
    marginBottom: 15,
  },
  relayButton: {
    borderWidth: 1,
    borderColor: '#11ad45bc',
    borderRadius: 15,
    paddingVertical: 8,
    paddingHorizontal: 10,
    marginVertical: 4,
    minWidth: '22%',
    alignItems: 'center',
  },
  relayButtonSelected: {
    backgroundColor: '#11ad45bc',
  },
  relayButtonText: {
    color: '#11ad45bc',
    fontWeight: 'bold',
  },
  relayButtonTextSelected: {
    color: 'white',
  },
  buttonContainer: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    width: '100%',
    marginTop: 20,
  },
  button: {
    borderRadius: 10,
    padding: 10,
    elevation: 2,
    width: '48%',
  },
  buttonClose: {
    backgroundColor: '#d9534f',
  },
  buttonSave: {
    backgroundColor: '#11ad45bc',
  },
  textStyle: {
    color: 'white',
    fontWeight: 'bold',
    textAlign: 'center',
  },
});
import { Ionicons } from '@expo/vector-icons';
import DateTimePicker from '@react-native-community/datetimepicker';
import { useRouter } from 'expo-router';
import { onValue, push, ref, remove, set, update, get } from 'firebase/database';
import React, { useEffect, useState } from 'react';
import {
  KeyboardAvoidingView,
  Modal,
  Platform,
  Pressable,
  ScrollView,
  StyleSheet,
  Switch,
  Text,
  TextInput,
  TouchableOpacity,
  View
} from 'react-native';
import { database } from '../../firebaseConfig.js';

const FIREBASE_CONFIG_ROOT = `smart_farm_iot/config`;
const TIMER_PATH = `${FIREBASE_CONFIG_ROOT}/timers`;
const CONTEXT_CONFIG_PATH = `${FIREBASE_CONFIG_ROOT}/context_logic`;
const NAME_CONFIG_PATH = `${FIREBASE_CONFIG_ROOT}/custom_names`;
const RAIN_ALERT_CONFIG_PATH = `${FIREBASE_CONFIG_ROOT}/node_thresholds/isRainAlertEnabled`;

type RelayKey = 'RL1' | 'RL2' | 'RL3' | 'RL4';
const DEFAULT_RELAYS: RelayKey[] = ['RL1', 'RL2', 'RL3', 'RL4'];

const NodeConfigScreen = () => {
  const router = useRouter();

  const [timers, setTimers] = useState<any[]>([]);
  const [customRelayNames, setCustomRelayNames] = useState<Record<string, string>>({});
  const [isRainAlertEnabled, setIsRainAlertEnabled] = useState(false);
  const [contexts, setContexts] = useState<any>({
    temp: { threshold: 35, relay: 'RL1', duration: 10, enabled: false, label: 'Nhiệt độ', icon: 'thermometer-outline' },
    soil: { threshold: 30, relay: 'RL2', duration: 15, enabled: false, label: 'Độ ẩm đất', icon: 'water-outline' },
    light: { threshold: 10, relay: 'RL3', duration: 20, enabled: false, label: 'Ánh sáng', icon: 'sunny-outline' },
  });

  const [timerModalVisible, setTimerModalVisible] = useState(false);
  const [editingTimer, setEditingTimer] = useState<any>(null);
  const [contextModalVisible, setContextModalVisible] = useState(false);
  const [editingContextKey, setEditingContextKey] = useState<string | null>(null);
  const [showTimePicker, setShowTimePicker] = useState(false);

  // --- SỬA LỖI: Kiểm tra an toàn cho toUpperCase ---
  const getDisplayRelayName = (key: any) => {
    if (!key || typeof key !== 'string') return 'N/A'; // Trả về N/A nếu key bị undefined/null
    const upperKey = key.toUpperCase();
    return customRelayNames[upperKey] || upperKey;
  };

  useEffect(() => {
    // 1. Lấy tên Custom với kiểm tra an toàn
    const unsubNames = onValue(ref(database, NAME_CONFIG_PATH), (snap) => {
      const data = snap.val() || {};
      const mapped: Record<string, string> = {};
      Object.keys(data).forEach(k => { 
        if (k) mapped[k.toUpperCase()] = data[k]; 
      });
      setCustomRelayNames(mapped);
    });

    // 2. Lấy Timers
    const unsubTimers = onValue(ref(database, TIMER_PATH), (snap) => {
      const data = snap.val() || {};
      setTimers(Object.keys(data).map(key => ({ id: key, ...data[key] })));
    });

    // 3. Lấy Ngữ cảnh
    get(ref(database, CONTEXT_CONFIG_PATH)).then((snap) => {
      if (snap.exists()) {
        const cloudData = snap.val();
        setContexts((prev: any) => {
          const newState = { ...prev };
          Object.keys(cloudData).forEach(key => {
            if (prev[key]) newState[key] = { ...prev[key], ...cloudData[key] };
          });
          return newState;
        });
      }
    });

    // 4. Lấy Cảnh báo mưa
    const unsubRain = onValue(ref(database, RAIN_ALERT_CONFIG_PATH), (snap) => {
      setIsRainAlertEnabled(snap.val() ?? false);
    });

    return () => { unsubNames(); unsubTimers(); unsubRain(); };
  }, []);

  const saveContext = () => {
    if (!editingContextKey) return;
    set(ref(database, `${CONTEXT_CONFIG_PATH}/${editingContextKey}`), contexts[editingContextKey])
      .then(() => setContextModalVisible(false));
  };

  const saveTimer = () => {
    if (!editingTimer) return;
    const data = { ...editingTimer, enabled: editingTimer.enabled ?? true };
    const promise = editingTimer.id 
      ? update(ref(database, `${TIMER_PATH}/${editingTimer.id}`), data)
      : push(ref(database, TIMER_PATH), data);
    promise.then(() => setTimerModalVisible(false));
  };

  return (
    <View style={styles.container}>
      <View style={styles.header}>
        <TouchableOpacity onPress={() => router.replace("/setting")} style={styles.backButton}>
          <Ionicons name="arrow-back" size={24} color="#333" />
          <Text style={styles.headerText}>Cấu hình Node</Text>
        </TouchableOpacity>
      </View>

      <ScrollView showsVerticalScrollIndicator={false}>
        {/* TIMER SECTION */}
        <View style={styles.sectionHeader}>
          <Text style={styles.sectionTitle}>Lịch trình Timer</Text>
          <TouchableOpacity onPress={() => { setEditingTimer({ name: 'Lịch mới', time: '08:00', duration: 10, relayKey: 'RL1' }); setTimerModalVisible(true); }}>
            <Ionicons name="add-circle" size={28} color="#11ad45bc" />
          </TouchableOpacity>
        </View>
        <View style={styles.card}>
          {timers.map(t => (
            <View key={t.id} style={styles.itemRow}>
              <TouchableOpacity style={{ flex: 1 }} onPress={() => { setEditingTimer(t); setTimerModalVisible(true); }}>
                <Text style={styles.itemTitle}>{t.name}</Text>
                {/* Sử dụng hàm an toàn ở đây */}
                <Text style={styles.itemSub}>{t.time} | {getDisplayRelayName(t.relayKey)} | {t.duration}s</Text>
              </TouchableOpacity>
              <Switch value={t.enabled} onValueChange={(v) => update(ref(database, `${TIMER_PATH}/${t.id}`), { enabled: v })} trackColor={{ true: '#11ad45bc' }} />
              <TouchableOpacity onPress={() => remove(ref(database, `${TIMER_PATH}/${t.id}`))} style={{ marginLeft: 10 }}><Ionicons name="trash-outline" size={20} color="red" /></TouchableOpacity>
            </View>
          ))}
        </View>

        {/* CONTEXT SECTION */}
        <Text style={[styles.sectionTitle, { marginTop: 20, marginBottom: 10 }]}>Ngữ cảnh thông minh</Text>
        <View style={styles.card}>
          {Object.keys(contexts).map((key) => (
            <View key={key} style={styles.itemRow}>
              <Ionicons name={contexts[key].icon} size={24} color="#11ad45bc" style={{ marginRight: 10 }} />
              <View style={{ flex: 1 }}>
                <Text style={styles.itemTitle}>{contexts[key].label}</Text>
                <Text style={styles.itemSub}>
                  Ngưỡng: {contexts[key].threshold} | {getDisplayRelayName(contexts[key].relay)}
                  {key !== 'light' && ` | Chạy ${contexts[key].duration}s`}
                </Text>
              </View>
              <Switch 
                value={contexts[key].enabled} 
                onValueChange={(v) => {
                  setContexts((p:any) => ({...p, [key]: {...p[key], enabled: v}}));
                  update(ref(database, `${CONTEXT_CONFIG_PATH}/${key}`), { enabled: v });
                }} 
                trackColor={{ true: '#11ad45bc' }} 
              />
              <TouchableOpacity style={{ marginLeft: 15 }} onPress={() => { setEditingContextKey(key); setContextModalVisible(true); }}>
                <Ionicons name="options-outline" size={26} color="#11ad45bc" />
              </TouchableOpacity>
            </View>
          ))}
        </View>
      </ScrollView>

      {/* MODAL CONTEXT */}
      <Modal visible={contextModalVisible} transparent animationType="fade">
        <View style={modalStyles.centeredView}>
          <View style={modalStyles.modalView}>
            {editingContextKey && (
              <ScrollView>
                <Text style={modalStyles.modalTitle}>Cài đặt {contexts[editingContextKey].label}</Text>
                
                <Text style={modalStyles.label}>Ngưỡng kích hoạt:</Text>
                <TextInput 
                  style={modalStyles.input} 
                  keyboardType="numeric" 
                  defaultValue={String(contexts[editingContextKey].threshold)} 
                  onChangeText={t => setContexts((p:any) => ({...p, [editingContextKey]: {...p[editingContextKey], threshold: parseInt(t)||0}}))} 
                />

                {editingContextKey !== 'light' && (
                  <>
                    <Text style={modalStyles.label}>Thời gian chạy (giây):</Text>
                    <TextInput 
                      style={modalStyles.input} 
                      keyboardType="numeric" 
                      defaultValue={String(contexts[editingContextKey].duration)} 
                      onChangeText={t => setContexts((p:any) => ({...p, [editingContextKey]: {...p[editingContextKey], duration: parseInt(t)||0}}))} 
                    />
                  </>
                )}

                <Text style={modalStyles.label}>Chọn thiết bị (Lưu mã Relay):</Text>
                <View style={modalStyles.relayGroup}>
                  {DEFAULT_RELAYS.map(r => (
                    <TouchableOpacity 
                      key={r} 
                      style={[modalStyles.relayBtn, contexts[editingContextKey].relay === r && modalStyles.relayBtnActive]} 
                      onPress={() => setContexts((p:any) => ({
                        ...p, 
                        [editingContextKey]: { ...p[editingContextKey], relay: r }
                      }))}
                    >
                      <Text style={{ color: contexts[editingContextKey].relay === r ? '#fff' : '#11ad45bc', fontWeight: 'bold' }}>
                        {getDisplayRelayName(r)}
                      </Text>
                    </TouchableOpacity>
                  ))}
                </View>

                <View style={modalStyles.btnRow}>
                  <Pressable style={[modalStyles.btn, { backgroundColor: '#ccc' }]} onPress={() => setContextModalVisible(false)}><Text style={styles.btnText}>Hủy</Text></Pressable>
                  <Pressable style={[modalStyles.btn, { backgroundColor: '#11ad45bc' }]} onPress={saveContext}><Text style={styles.btnText}>Lưu</Text></Pressable>
                </View>
              </ScrollView>
            )}
          </View>
        </View>
      </Modal>

      {/* MODAL TIMER */}
      <Modal visible={timerModalVisible} transparent animationType="slide">
        <KeyboardAvoidingView behavior={Platform.OS === 'ios' ? 'padding' : 'height'} style={{ flex: 1 }}>
          <View style={modalStyles.centeredView}>
            <View style={modalStyles.modalView}>
              <ScrollView>
                <Text style={modalStyles.modalTitle}>{editingTimer?.id ? "Sửa Lịch" : "Thêm Lịch"}</Text>
                
                <Text style={modalStyles.label}>Tên lịch trình:</Text>
                <TextInput style={modalStyles.input} value={editingTimer?.name} onChangeText={t => setEditingTimer((p:any) => ({...p, name: t}))} />
                
                <TouchableOpacity style={modalStyles.timeSelector} onPress={() => setShowTimePicker(true)}>
                  <Text style={modalStyles.timeText}>{editingTimer?.time || "08:00"}</Text>
                  <Ionicons name="time-outline" size={24} color="#11ad45bc" />
                </TouchableOpacity>

                {showTimePicker && (
                  <DateTimePicker value={new Date()} mode="time" is24Hour display="default" onChange={(e, d) => {
                    setShowTimePicker(false);
                    if (d) {
                      const time = `${d.getHours().toString().padStart(2,'0')}:${d.getMinutes().toString().padStart(2,'0')}`;
                      setEditingTimer((p:any) => ({...p, time}));
                    }
                  }} />
                )}

                <Text style={modalStyles.label}>Thời lượng (s):</Text>
                <TextInput style={modalStyles.input} keyboardType="numeric" value={String(editingTimer?.duration || '')} onChangeText={t => setEditingTimer((p:any) => ({...p, duration: parseInt(t)||0}))} />

                <Text style={modalStyles.label}>Thiết bị:</Text>
                <View style={modalStyles.relayGroup}>
                  {DEFAULT_RELAYS.map(r => (
                    <TouchableOpacity 
                      key={r} 
                      style={[modalStyles.relayBtn, editingTimer?.relayKey === r && modalStyles.relayBtnActive]} 
                      onPress={() => setEditingTimer((p:any) => ({...p, relayKey: r}))}
                    >
                      <Text style={{ color: editingTimer?.relayKey === r ? '#fff' : '#11ad45bc', fontWeight: 'bold' }}>
                        {getDisplayRelayName(r)}
                      </Text>
                    </TouchableOpacity>
                  ))}
                </View>

                <View style={modalStyles.btnRow}>
                  <Pressable style={[modalStyles.btn, { backgroundColor: '#ccc' }]} onPress={() => setTimerModalVisible(false)}><Text style={styles.btnText}>Hủy</Text></Pressable>
                  <Pressable style={[modalStyles.btn, { backgroundColor: '#11ad45bc' }]} onPress={saveTimer}><Text style={styles.btnText}>Lưu</Text></Pressable>
                </View>
              </ScrollView>
            </View>
          </View>
        </KeyboardAvoidingView>
      </Modal>
    </View>
  );
};

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f0f2f5', padding: 15 },
  header: { paddingTop: 40, marginBottom: 15 },
  backButton: { flexDirection: 'row', alignItems: 'center' },
  headerText: { fontSize: 18, fontWeight: 'bold', marginLeft: 5 },
  sectionHeader: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center', marginBottom: 10 },
  sectionTitle: { fontSize: 16, fontWeight: 'bold', color: '#444' },
  card: { backgroundColor: '#fff', borderRadius: 12, padding: 15, elevation: 2, marginBottom: 10 },
  itemRow: { flexDirection: 'row', alignItems: 'center', paddingVertical: 12, borderBottomWidth: 1, borderBottomColor: '#f0f0f0' },
  itemTitle: { fontSize: 15, fontWeight: 'bold' },
  itemSub: { fontSize: 12, color: '#777' },
  btnText: { color: '#fff', fontWeight: 'bold', textAlign: 'center' }
});

const modalStyles = StyleSheet.create({
  centeredView: { flex: 1, justifyContent: 'center', alignItems: 'center', backgroundColor: 'rgba(0,0,0,0.5)' },
  modalView: { width: '90%', maxHeight: '85%', backgroundColor: '#fff', borderRadius: 20, padding: 20 },
  modalTitle: { fontSize: 18, fontWeight: 'bold', color: '#11ad45bc', marginBottom: 15 },
  label: { fontSize: 13, fontWeight: '600', marginTop: 12, color: '#666' },
  input: { borderWidth: 1, borderColor: '#ddd', borderRadius: 8, padding: 10, marginTop: 5 },
  timeSelector: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center', borderWidth: 1, borderColor: '#11ad45bc', borderRadius: 10, padding: 12, marginTop: 15, backgroundColor: '#f0fdf4' },
  timeText: { fontSize: 20, fontWeight: 'bold', color: '#333' },
  relayGroup: { flexDirection: 'row', flexWrap: 'wrap', gap: 8, marginTop: 10 },
  relayBtn: { padding: 10, borderWidth: 1, borderColor: '#11ad45bc', borderRadius: 8, minWidth: '46%', alignItems: 'center' },
  relayBtnActive: { backgroundColor: '#11ad45bc' },
  btnRow: { flexDirection: 'row', justifyContent: 'space-between', marginTop: 25 },
  btn: { flex: 0.48, padding: 12, borderRadius: 10 }
});

export default NodeConfigScreen;
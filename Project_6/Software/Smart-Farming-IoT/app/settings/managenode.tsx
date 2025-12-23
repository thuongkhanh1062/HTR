import { Ionicons } from '@expo/vector-icons';
import { useRouter } from 'expo-router';
import { child, onValue, ref, set } from 'firebase/database';
import React, { useEffect, useState } from 'react';
import { ActivityIndicator, Alert, KeyboardAvoidingView, Platform, ScrollView, StyleSheet, Text, TextInput, TouchableOpacity, View } from 'react-native';
import { database } from 'E:/HTR/Project_6/Software/Smart-Farming-IoT/firebaseConfig.js';

const NAME_CONFIG_PATH = 'smart_farm_iot/user_data/user_1/config/custom_names';
const dbRef = ref(database);

const DEFAULT_NAMES = {
  node1: 'Node Sensor',
  node2: 'Node HMI',
  node3: 'Node Power',
  relay1: 'RL1',
  relay2: 'RL2',
  relay3: 'RL3',
  relay4: 'RL4',
};
interface NameInputProps {
  label: string;
  value: string;
  onChangeText: (text: string) => void;
  placeholder: string;
}
const NameInput = ({ label, value, onChangeText, placeholder }: NameInputProps) => (
  <View style={styles.inputRow}>
    <Text style={styles.inputLabel}>{label}</Text>
    <TextInput
      style={styles.textInput}
      onChangeText={onChangeText}
      value={value}
      placeholder={placeholder}
    />
  </View>
);

const ManageNodeScreen = () => {
  const router = useRouter();
  const [names, setNames] = useState(DEFAULT_NAMES);
  const [isLoading, setIsLoading] = useState(true);

  useEffect(() => {
    const unsubscribe = onValue(child(dbRef, NAME_CONFIG_PATH), (snapshot) => {
      if (snapshot.exists()) {
        const loadedNames = snapshot.val();
        setNames({ ...DEFAULT_NAMES, ...loadedNames });
      } else {
        setNames(DEFAULT_NAMES);
      }
      setIsLoading(false);
    }, (error) => {
      console.error("Lỗi khi tải tên cấu hình:", error);
      Alert.alert("Lỗi", "Không thể tải cấu hình tên từ Firebase.");
      setIsLoading(false);
    });
    return () => unsubscribe();
  }, []);

  const onBackToSetting = () => {
    router.replace("/setting");
  };

  const handleSaveNames = () => {
    setIsLoading(true);
    set(ref(database, NAME_CONFIG_PATH), names)
      .then(() => {
        Alert.alert("Thành công", "Đã lưu thành công các tên tùy chỉnh!");
        setIsLoading(false);
      })
      .catch((error) => {
        console.error("Lỗi khi lưu tên vào Firebase:", error);
        Alert.alert("Lỗi", "Không thể lưu cấu hình tên.");
        setIsLoading(false);
      });
  };
  return (
    <View style={styles.container}>
      <View style={styles.header}>
        <TouchableOpacity onPress={onBackToSetting} style={styles.backButton}>
          <Ionicons name="arrow-back" size={24} color="#333" />
          <Text style={styles.headerText}>Cài đặt</Text>
        </TouchableOpacity>
      </View>
      <Text style={styles.title}>QUẢN LÝ TÊN NODE VÀ RELAY</Text>

      <KeyboardAvoidingView
        style={styles.keyboardAvoidingContainer}
        behavior={Platform.OS === 'ios' ? 'padding' : 'height'}
        keyboardVerticalOffset={Platform.OS === 'ios' ? 40 : 0}
      >
        <ScrollView contentContainerStyle={styles.scrollContent}>
          {isLoading ? (
            <ActivityIndicator size="large" color="#11ad45bc" style={{ marginTop: 50 }} />
          ) : (
            <View>
              <Text style={styles.sectionTitle}>Tên Node Cảm Biến</Text>
              <NameInput
                label="Node Sensor (Node 1)"
                value={names.node1}
                onChangeText={(text) => setNames({ ...names, node1: text })}
                placeholder={DEFAULT_NAMES.node1}
              />
              <NameInput
                label="HMI Node (Node 2)"
                value={names.node2}
                onChangeText={(text) => setNames({ ...names, node2: text })}
                placeholder={DEFAULT_NAMES.node2}
              />
              <NameInput
                label="Node Power (Node 3)"
                value={names.node3}
                onChangeText={(text) => setNames({ ...names, node3: text })}
                placeholder={DEFAULT_NAMES.node3}
              />
              <Text style={styles.sectionTitle}>Tên Relay (4 Ngõ ra)</Text>
              <NameInput
                label="Relay 1"
                value={names.relay1}
                onChangeText={(text) => setNames({ ...names, relay1: text })}
                placeholder={DEFAULT_NAMES.relay1}
              />
              <NameInput
                label="Relay 2"
                value={names.relay2}
                onChangeText={(text) => setNames({ ...names, relay2: text })}
                placeholder={DEFAULT_NAMES.relay2}
              />
              <NameInput
                label="Relay 3"
                value={names.relay3}
                onChangeText={(text) => setNames({ ...names, relay3: text })}
                placeholder={DEFAULT_NAMES.relay3}
              />
              <NameInput
                label="Relay 4"
                value={names.relay4}
                onChangeText={(text) => setNames({ ...names, relay4: text })}
                placeholder={DEFAULT_NAMES.relay4}
              />
            </View>
          )}
        </ScrollView>
      </KeyboardAvoidingView>
      <TouchableOpacity
        onPress={handleSaveNames}
        style={[styles.saveButton, isLoading && { opacity: 0.5 }]}
        disabled={isLoading}
      >
        <Ionicons name="save-outline" size={24} color="#fff" />
        <Text style={styles.saveButtonText}>LƯU CẤU HÌNH</Text>
      </TouchableOpacity>
    </View>
  );
};

export default ManageNodeScreen;

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
  scrollContent: {
    paddingBottom: 20,
  },
  sectionTitle: {
    fontSize: 18,
    fontWeight: 'bold',
    color: '#11ad45bc',
    marginTop: 15,
    marginBottom: 10,
    borderBottomWidth: 1,
    borderBottomColor: '#eee',
    paddingBottom: 5,
  },
  inputRow: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    backgroundColor: '#fff',
    padding: 10,
    borderRadius: 8,
    marginBottom: 10,
  },
  inputLabel: {
    fontSize: 16,
    color: '#333',
    fontWeight: '500',
    flex: 1,
  },
  textInput: {
    flex: 2,
    borderWidth: 1,
    borderColor: '#ddd',
    borderRadius: 5,
    padding: 8,
    fontSize: 16,
    marginLeft: 10,
  },
  saveButton: {
    flexDirection: 'row',
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: '#11ad45bc',
    padding: 15,
    borderRadius: 10,
    marginTop: 10,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 4 },
    shadowOpacity: 0.3,
    shadowRadius: 5,
    elevation: 5,
  },
  saveButtonText: {
    color: '#fff',
    fontSize: 18,
    fontWeight: 'bold',
    marginLeft: 10,
  }
  ,
  keyboardAvoidingContainer: {
    flex: 1,
  },
});
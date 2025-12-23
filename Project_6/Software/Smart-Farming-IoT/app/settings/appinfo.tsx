import { Ionicons } from '@expo/vector-icons';
import { useRouter } from 'expo-router';
import { onValue, ref } from 'firebase/database';
import React, { useEffect, useState } from 'react';
import { ActivityIndicator, StyleSheet, Text, TouchableOpacity, View } from 'react-native';
import { database } from 'E:/HTR/Project_6/Software/Smart-Farming-IoT/firebaseConfig.js';

const USER_ID = 'user_1';
const FIREBASE_CONFIG_ROOT = `smart_farm_iot/user_data/${USER_ID}/config/appinfo`;

const AppInfoScreen = () => {
  const router = useRouter();
  const [applicationName, setApplicationName] = useState('');
  const [applicationVersion, setApplicationVersion] = useState('N/A');
  const [applicationAuthor, setApplicationAuthor] = useState('N/A');
  const [applicationDate, setApplicationDate] = useState('N/A');
  const [isLoading, setIsLoading] = useState(true);
  const [error, setError] = useState('');

  const onBackToSetting = () => {
    router.replace("/setting");
  };

  useEffect(() => {
    const appInfoRef = ref(database, FIREBASE_CONFIG_ROOT);

    const unsubscribe = onValue(appInfoRef, (snapshot) => {
      if (snapshot.exists()) {
        const value = snapshot.val();

        setApplicationName(value.name || 'Smart Farm IoT App');
        setApplicationVersion(value.version || 'N/A');
        setApplicationAuthor(value.author || 'N/A');
        setApplicationDate(value.date || 'N/A');

        console.log("Thông tin ứng dụng đã cập nhật: ", value);
      } else {
        setError("Không tìm thấy thông tin ứng dụng tại đường dẫn Firebase.");
      }
      setIsLoading(false);
    }, (dbError) => {
      console.error("Lỗi đọc Firebase: ", dbError);
      setError("Lỗi kết nối hoặc đọc dữ liệu Firebase.");
      setIsLoading(false);
    });

    return () => {
      console.log("Ngừng lắng nghe Firebase...");
      unsubscribe();
    };
  }, []);

  if (isLoading) {
    return (
      <View style={[styles.container, { justifyContent: 'center', alignItems: 'center' }]}>
        <ActivityIndicator size="large" color="#11ad45bc" />
        <Text>Đang tải thông tin ứng dụng...</Text>
      </View>
    );
  }

  if (error) {
    return (
      <View style={[styles.container, { justifyContent: 'center', alignItems: 'center' }]}>
        <Text style={{ color: 'red', fontSize: 16 }}>Lỗi: {error}</Text>
      </View>
    );
  }


  return (
    <View style={styles.container}>
      <View style={styles.header}>
        <TouchableOpacity onPress={onBackToSetting} style={styles.backButton}>
          <Ionicons name="arrow-back" size={24} color="#333" />
          <Text style={styles.headerText}>Cài đặt</Text>
        </TouchableOpacity>
      </View>

      <View style={styles.contentContainer}>
        <Text style={styles.appName}>{applicationName}</Text>
        <View style={styles.detailRow}>
          <Ionicons name="calendar-outline" size={24} color="#555" style={styles.iconStyle} />
          <Text style={styles.appDetail}>Ngày: {applicationDate}</Text>
        </View>
        <View style={styles.detailRow}>
          <Ionicons name="person-circle-outline" size={24} color="#555" style={styles.iconStyle} />
          <Text style={styles.appDetail}>Tác giả: {applicationAuthor}</Text>
        </View>
        <View style={styles.detailRow}>
          <Ionicons name="git-branch-outline" size={24} color="#555" style={styles.iconStyle} />
          <Text style={styles.appDetail}>Phiên bản: {applicationVersion}</Text>
        </View>
        <View style={styles.appIconContainer}>
          <Ionicons name="leaf-outline" size={80} color="#11ad45bc" />
        </View>
      </View>
    </View>
  )
}

export default AppInfoScreen

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
    marginBottom: 20,
    borderBottomWidth: 1,
    borderBottomColor: '#eee',
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
  contentContainer: {
    marginLeft: "10%",
    paddingTop: 20,
  },
  appName: {
    fontSize: 32,
    fontWeight: "bold",
    color: '#11ad45bc',
    marginBottom: 20,
  },
  detailRow: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: 10,
  },
  iconStyle: {
    marginRight: 10,
  },
  appDetail: {
    fontSize: 18,
    color: '#555',
  },
  appIconContainer: {
    marginTop: 40,
    alignItems: 'center',
    marginLeft: -50,
  }
})
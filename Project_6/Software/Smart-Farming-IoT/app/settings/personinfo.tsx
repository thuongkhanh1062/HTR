import { Ionicons } from '@expo/vector-icons';
import AsyncStorage from '@react-native-async-storage/async-storage';
import { useRouter } from 'expo-router';
import { child, get, ref, update } from 'firebase/database';
import React, { useEffect, useState } from 'react';
import { ActivityIndicator, Alert, KeyboardAvoidingView, Platform, ScrollView, StyleSheet, Text, TextInput, TouchableOpacity, View } from 'react-native';
import { database } from 'E:/HTR/Project_6/Software/Smart-Farming-IoT/firebaseConfig.js';

const FIREBASE_USER_ROOT_PATH = `smart_farm_iot/user_data`;
const dbRef = ref(database);

const getUserId = async (): Promise<string | null> => {
    try {
        const userId = await AsyncStorage.getItem('loggedInUserId');
        return userId;
    } catch (error) {
        return null;
    }
};

interface FormData {
    email: string;
    id: string;
    name: string;
    password: string;
    repassword: string;
}

const PersonInfo = () => {
    const router = useRouter();
    const [originalUserData, setOriginalUserData] = useState(null);
    const [formData, setFormData] = useState<FormData>({ 
        email: '', 
        id: '', 
        name: '', 
        password: '', 
        repassword: '' 
    });
    const [currentUserIdKey, setCurrentUserIdKey] = useState<string | null>(null);
    const [isLoading, setIsLoading] = useState(true);
    const [isUpdating, setIsUpdating] = useState(false);

    useEffect(() => {
        fetchCurrentUser();
    }, []);

    const fetchCurrentUser = async () => {
        setIsLoading(true);
        const userIdKey = await getUserId();
        if (!userIdKey) {
            Alert.alert("Lỗi", "Phiên đăng nhập hết hạn.");
            router.replace("/login");
            return;
        }

        try {
            const userPath = `${FIREBASE_USER_ROOT_PATH}/${userIdKey}`;
            const snapshot = await get(child(dbRef, userPath));
            
            if (snapshot.exists()) {
                const data = snapshot.val();
                setOriginalUserData(data);
                setCurrentUserIdKey(userIdKey);
                setFormData({
                    email: data.email,
                    id: data.id,
                    name: data.name,
                    password: '',
                    repassword: ''
                });
            } else {
                Alert.alert("Lỗi", "Không tìm thấy dữ liệu người dùng.");
            }
        } catch (error) {
            Alert.alert("Lỗi hệ thống", "Không thể tải dữ liệu.");
        } finally {
            setIsLoading(false);
        }
    };
    
    const checkDuplicate = async (field: keyof FormData, value: string, usersData: any, currentKey: string | null): Promise<boolean> => {
        if (originalUserData && value === originalUserData[field]) {
            return false;
        }
        for (const key in usersData) {
            if (key !== currentKey && usersData[key][field] === value) {
                return true;
            }
        }
        return false;
    };

    const onUpdate = async () => {
        if (isUpdating) return;
        setIsUpdating(true);

        const { email, id, name, password, repassword } = formData;
        
        if (!email || !id || !name) {
            Alert.alert("Lỗi", "Email, ID và Tên không được để trống.");
            setIsUpdating(false);
            return;
        }
        
        if (password) {
            if (password.length < 6) {
                Alert.alert("Lỗi", "Mật khẩu phải có ít nhất 6 ký tự.");
                setIsUpdating(false);
                return;
            }
            if (password !== repassword) {
                Alert.alert("Lỗi", "Mật khẩu nhập lại không khớp.");
                setIsUpdating(false);
                return;
            }
        }

        try {
            const snapshot = await get(child(dbRef, FIREBASE_USER_ROOT_PATH));
            const usersData = snapshot.val();

            if (await checkDuplicate('email', email, usersData, currentUserIdKey)) {
                Alert.alert("Lỗi", "Email này đã được sử dụng bởi tài khoản khác.");
                setIsUpdating(false);
                return;
            }
            if (await checkDuplicate('id', id, usersData, currentUserIdKey)) {
                Alert.alert("Lỗi", "ID này đã được sử dụng bởi tài khoản khác.");
                setIsUpdating(false);
                return;
            }
            if (await checkDuplicate('name', name, usersData, currentUserIdKey)) {
                Alert.alert("Lỗi", "Tên đăng nhập này đã được sử dụng bởi tài khoản khác.");
                setIsUpdating(false);
                return;
            }

            const updates = {
                email: email,
                id: id,
                name: name,
                ...(password && { password: password }),
            };
            
            const userPath = `${FIREBASE_USER_ROOT_PATH}/${currentUserIdKey}`;
            await update(child(dbRef, userPath), updates);

            Alert.alert("Thành công", "Thông tin cá nhân đã được cập nhật!");
            fetchCurrentUser(); 

        } catch (error) {
            console.error("Lỗi khi cập nhật dữ liệu:", error);
            Alert.alert("Lỗi hệ thống", "Không thể cập nhật dữ liệu lên Firebase.");
        } finally {
            setIsUpdating(false);
        }
    };
    
    const onBackToSetting = () => {
        router.replace("/setting");
    };
    
    const handleChange = (field: keyof FormData, value: string): void => {
      setFormData(prev => ({ ...prev, [field]: value }));
    };

    if (isLoading) {
        return (
            <View style={styles.loadingContainer}>
                <ActivityIndicator size="large" color="#11ad45bc" />
                <Text style={styles.loadingText}>Đang tải thông tin cá nhân...</Text>
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

            <KeyboardAvoidingView
                style={styles.keyboardAvoidingContainer}
                behavior={Platform.OS === 'ios' ? 'padding' : 'height'}
                keyboardVerticalOffset={Platform.OS === 'ios' ? 0 : 0} 
            >
                <ScrollView 
                    contentContainerStyle={styles.scrollContent}
                    showsVerticalScrollIndicator={false}
                >
                    <View style={styles.contentContainer}>
                        <Text style={styles.appName}>Thông tin cá nhân</Text>
                        <View style={styles.formContainer}>
                            <Text style={styles.inputLabel}>Email:</Text>
                            <TextInput
                                style={styles.input}
                                value={formData.email}
                                onChangeText={(text) => handleChange('email', text)}
                                keyboardType="email-address"
                            />

                            <Text style={styles.inputLabel}>ID:</Text>
                            <TextInput
                                style={styles.input}
                                value={formData.id}
                                onChangeText={(text) => handleChange('id', text)}
                            />

                            <Text style={styles.inputLabel}>Name (Tên đăng nhập):</Text>
                            <TextInput
                                style={styles.input}
                                value={formData.name}
                                onChangeText={(text) => handleChange('name', text)}
                            />

                            <Text style={styles.inputLabel}>Mật khẩu mới (Không bắt buộc):</Text>
                            <TextInput
                                style={styles.input}
                                value={formData.password}
                                onChangeText={(text) => handleChange('password', text)}
                                secureTextEntry
                                placeholder="Nhập mật khẩu mới"
                            />

                            <Text style={styles.inputLabel}>Nhập lại Mật khẩu:</Text>
                            <TextInput
                                style={styles.input}
                                value={formData.repassword}
                                onChangeText={(text) => handleChange('repassword', text)}
                                secureTextEntry
                                placeholder="Nhập lại mật khẩu mới"
                            />

                            <TouchableOpacity 
                                style={styles.updateButton} 
                                onPress={onUpdate}
                                disabled={isUpdating}
                            >
                                {isUpdating ? (
                                    <ActivityIndicator color="#fff" />
                                ) : (
                                    <Text style={styles.updateButtonText}>Cập nhật thông tin</Text>
                                )}
                            </TouchableOpacity>
                        </View>
                    </View>
                </ScrollView>
            </KeyboardAvoidingView>
        </View>
    );
};

export default PersonInfo;

const styles = StyleSheet.create({
    container: {
        flex: 1,
        backgroundColor: '#f5f5f5',
        padding: 15,
    },
    loadingContainer: {
        flex: 1,
        justifyContent: 'center',
        alignItems: 'center',
    },
    loadingText: {
        marginTop: 10,
        fontSize: 16,
        color: '#555',
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
    keyboardAvoidingContainer: {
        flex: 1, 
    },
    scrollContent: {
        flexGrow: 1,
        paddingBottom: 20, 
    },
    contentContainer: {
        // paddingHorizontal: 10, (Đã có padding 15 ở container gốc)
    },
    appName: {
        fontSize: 24,
        fontWeight: "bold",
        color: '#11ad45bc',
        marginBottom: 20,
        textAlign: 'center',
    },
    formContainer: {
        backgroundColor: '#fff',
        padding: 20,
        borderRadius: 10,
        elevation: 2,
    },
    inputLabel: {
        fontSize: 16,
        fontWeight: 'bold',
        color: '#333',
        marginTop: 10,
        marginBottom: 5,
    },
    input: {
        borderWidth: 1,
        borderColor: '#ccc',
        borderRadius: 8,
        padding: 12,
        fontSize: 16,
        marginBottom: 10,
    },
    updateButton: {
        backgroundColor: '#11ad45bc',
        padding: 15,
        borderRadius: 8,
        alignItems: 'center',
        marginTop: 20,
    },
    updateButtonText: {
        color: '#fff',
        fontSize: 18,
        fontWeight: 'bold',
    },
});
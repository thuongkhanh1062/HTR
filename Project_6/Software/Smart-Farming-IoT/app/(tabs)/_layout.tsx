
import { Tabs } from 'expo-router';
import React from 'react';
import { Image, View } from 'react-native';

const HomeImg = require('../../assets/images/home.png');
const DocImg = require('../../assets/images/document.png');
const SettingImg = require('../../assets/images/settings.png');
const BellImg = require('../../assets/images/bell.png');

export default function TabLayout() {
    return (
        <Tabs
            screenOptions={{
                headerShown: false,
                tabBarShowLabel: false,
                tabBarStyle: {
                    backgroundColor: '#ffffff',
                    height: "10%",
                    paddingTop: 10,
                    borderTopLeftRadius: 20,
                    borderTopRightRadius: 20,
                },
            }}
        >
            <Tabs.Screen
                name="home"
                options={{
                    title: "Trang chủ",
                    tabBarIcon: ({ focused }) => (
                        <View style={{ alignItems: 'center', justifyContent: 'center', opacity: focused ? 1 : 0.5 }}>
                            <Image
                                source={HomeImg}
                                style={{ width: 32, height: 32, tintColor: focused ? '#11ad45' : 'gray' }}
                            />
                        </View>
                    ),
                }}
            />

            <Tabs.Screen
                name="about"
                options={{
                    title: "Cảm biến",
                    tabBarIcon: ({ focused }) => (
                        <View style={{ alignItems: 'center', justifyContent: 'center', opacity: focused ? 1 : 0.5 }}>
                            <Image
                                source={DocImg}
                                style={{ width: 32, height: 32, tintColor: focused ? '#11ad45' : 'gray' }}
                            />
                        </View>
                    ),
                }}
            />

            <Tabs.Screen
                name="notification"
                options={{
                    title: "thông báo",
                    tabBarIcon: ({ focused }) => (
                        <View style={{ alignItems: 'center', justifyContent: 'center', opacity: focused ? 1 : 0.5 }}>
                            <Image
                                source={BellImg}
                                style={{ width: 32, height: 32, tintColor: focused ? '#11ad45' : 'gray' }}
                            />
                        </View>
                    )
                }}
            />
            
            <Tabs.Screen
                name="setting"
                options={{
                    title: "cài đặt",
                    tabBarIcon: ({ focused }) => (
                        <View style={{ alignItems: 'center', justifyContent: 'center', opacity: focused ? 1 : 0.5 }}>
                            <Image
                                source={SettingImg}
                                style={{ width: 32, height: 32, tintColor: focused ? '#11ad45' : 'gray' }}
                            />
                        </View>
                    )
                }}
            />
        </Tabs>
    );
}
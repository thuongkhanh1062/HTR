import { initializeApp } from 'firebase/app';
import { getDatabase } from 'firebase/database';

// const firebaseConfig = {
//   apiKey: "AIzaSyAD4bX52_L7ENh7ax9VqqB2ex3t9VngELU",
//   authDomain: "smartfarmingiot-d4e85.firebaseapp.com",
//   databaseURL: "https://smartfarmingiot-d4e85-default-rtdb.asia-southeast1.firebasedatabase.app",
//   projectId: "smartfarmingiot-d4e85",
//   storageBucket: "smartfarmingiot-d4e85.firebasestorage.app",
//   messagingSenderId: "927803529662",
//   appId: "1:927803529662:web:2ee981ff9baf819941c1ef"
// };
const firebaseConfig = {
  apiKey: "AIzaSyAgVge-VC6S9RocrJQibOWFqY1De7nz3eM",
  authDomain: "smartfarmingiot-78911.firebaseapp.com",
  databaseURL: "https://smartfarmingiot-78911-default-rtdb.asia-southeast1.firebasedatabase.app",
  projectId: "smartfarmingiot-78911",
  storageBucket: "smartfarmingiot-78911.firebasestorage.app",
  messagingSenderId: "667011625904",
  appId: "1:667011625904:web:f83324d96295f65c5d1c75"
};

const app = initializeApp(firebaseConfig);
const database = getDatabase(app);
export { database };

// Import the functions you need from the SDKs you need
// import { initializeApp } from "firebase/app";
// TODO: Add SDKs for Firebase products that you want to use
// https://firebase.google.com/docs/web/setup#available-libraries

// Your web app's Firebase configuration
// Initialize Firebase
// const app = initializeApp(firebaseConfig);
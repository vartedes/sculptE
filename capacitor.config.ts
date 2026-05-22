import { CapacitorConfig } from '@capacitor/cli';

const config: CapacitorConfig = {
  appId: 'com.sculptengine.app',
  appName: 'SculptEngine',
  webDir: 'dist',
  server: {
    androidScheme: 'https'
  }
};

export default config;

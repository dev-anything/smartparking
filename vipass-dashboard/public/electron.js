//const { app, BrowserWindow } = require('electron');
//const isDev = require('electron-is-dev'); // 개발 환경인지 확인하기 위한 모듈
//const path = require('path'); // 경로 관련 유틸리티 모듈
import { app, BrowserWindow } from 'electron';
import isDev from 'electron-is-dev';
import path from 'path';

// 메인 창 변수 선언
let mainWindow = null;

// 새 창을 생성하는 함수 정의
function createWindow() {
  // 브라우저 창 생성
  mainWindow = new BrowserWindow({
    // 창 크기 설정
    width: 1200,
    height: 800,
    webPreferences: {
      nodeIntegration: true,
      contextIsolation: false,
      devTools: isDev,
    },
  });

  // 개발 모드에서는 로컬 서버로 연결
  // 프로덕션 모드에서는 빌드된 리액트 앱 파일 로드
  mainWindow.loadURL(isDev ? 'http://localhost:5173' : `file://${path.join(__dirname, '../build/index.html')}`);

  // 개발 모드에서 개발자 도구를 자동으로 열림
  if (isDev) {
    mainWindow.webContents.openDevTools({ mode: 'detach' });
  }

  mainWindow.setResizable(true);
  mainWindow.on('closed', () => (mainWindow = null));
  mainWindow.focus();
}

// 애플리케이션이 준비되면 새 창 생성
app.on('ready', createWindow);

// 모든 창이 닫히면
app.on('window-all-closed', function () {
  // 플랫폼이 맥이 아니면 애플리케이션 종료
  if (process.platform !== 'darwin') app.quit();
});
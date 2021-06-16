# Smart Bench Press

## 기능

### 1. Counting
벤치프레스를 1회 수행할 때마다 패시브 부저를 울리고, LCD에 횟수를 표시해 준다.

### 2. Balancing
바벨의 수평을 감지해주어 기울어질 경우 기울어진 방향의 패시브 부저가 울린다.

### 3. Emergeny Alarming
바벨에 깔려 위험한 상황이 발생한 경우 패시브 부저를 울려 비상 상황임을 표현한다.

## 사용한 센서

### 1. Counting Module
1. 1602LCD
2. 버튼
3. 조도센서(with ADC)
   
### 2. Balancing Module
1. 가속도계
2. 패시브 부저 2개
   
### 3. Emergency Alarming Module 
1. 압력센서(with ADC)

## Contributer
### 신동훈 (xldk78@ajou.ac.kr)
Counting Module 구현
### 정재욱 (br12345678@ajou.ac.kr)
Emergency Alarming Module 구현
### 민경현 (khmin1104@ajou.ac.kr)
Balancing Module 구현
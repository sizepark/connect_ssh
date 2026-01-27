# 컴파일러 및 플래그
CC = gcc
# -Wall: 모든 경고를 활성화합니다.
# -g: 디버깅 정보를 포함합니다.
CFLAGS = -Wall -g
# 링크할 라이브러리를 지정합니다.
LDFLAGS = -lssh

# 소스 파일 및 실행 파일
SOURCES = bandit_bot.c
TARGET = bandit_bot_make

# 기본 빌드 규칙
all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

# 빌드 결과물 삭제
clean:
	rm -f $(TARGET)
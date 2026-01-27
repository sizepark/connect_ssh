#include <stdio.h>
#include <stdlib.h>
#include <libssh/libssh.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <time.h>
#include <string.h>

#define TARGET_USER "bandit0"
#define TARGET_PASS "bandit0"
#define TARGET_HOST "bandit.labs.overthewire.org"
#define TARGET_PORT 2220

// Function prototype for the interactive shell
int interactive_shell(ssh_session session, ssh_channel channel, const char *initial_command);

int main(int argc, char *argv[]){
	ssh_session m_session = NULL;
	ssh_channel m_channel = NULL;
	int rc; // return code
	int port = TARGET_PORT;
	int exit_code = 0;

	m_session = ssh_new();
	if (m_session == NULL) {
		fprintf(stderr, "Failed to create SSH session.\n");
		return -1;
	}

	ssh_options_set(m_session, SSH_OPTIONS_HOST, TARGET_HOST);
	ssh_options_set(m_session, SSH_OPTIONS_PORT, &port);
	ssh_options_set(m_session, SSH_OPTIONS_USER, TARGET_USER);
	
	printf("[+] Connecting to %s on port %d...\n", TARGET_HOST, port);
	rc = ssh_connect(m_session);
	if (rc != SSH_OK) {
		fprintf(stderr, "Failed to connect: %s\n", ssh_get_error(m_session));
		exit_code = -1;
		goto cleanup;
	}

	rc = ssh_userauth_password(m_session, NULL, TARGET_PASS);
	if (rc != SSH_AUTH_SUCCESS) {
		fprintf(stderr, "Failed to authenticate: %s\n", ssh_get_error(m_session));
		exit_code = -1;
		goto cleanup;
	}

	printf("[+] Authentication successful.\n");

	m_channel = ssh_channel_new(m_session);
	if (m_channel == NULL) {
		fprintf(stderr, "Failed to create channel.\n");
		exit_code = -1;
		goto cleanup;
	}

	rc = ssh_channel_open_session(m_channel);
	if (rc != SSH_OK) {
		fprintf(stderr, "Failed to open channel session: %s\n", ssh_get_error(m_session));
		exit_code = -1;
		goto cleanup;
	}
	
	const char *command_to_run = NULL;
	if (argc > 1) {
		// Use the first argument as an initial command to run
		command_to_run = argv[1];
	}

	// Call the interactive shell function
	rc = interactive_shell(m_session, m_channel, command_to_run);
	if (rc != 0) {
		exit_code = -1;
	}

cleanup:
	printf("\n[+] Closing session.\n");
	if (m_channel) {
		ssh_channel_send_eof(m_channel);
		ssh_channel_free(m_channel); // 이 함수는 채널을 닫는 동작도 포함합니다.
	}
	if (m_session) {
		ssh_disconnect(m_session);
		ssh_free(m_session);
	}
	return exit_code;
}

/**
 * @brief 원격 서버와 상호작용하는 셸을 시작합니다.
 * 
 * @param session 활성화된 ssh_session.
 * @param channel 열린 ssh_channel.
 * @return 성공 시 0, 오류 발생 시 -1.
 * 
 * 이 함수는 다음을 수행해야 합니다:
 * 1. 원격 서버에 가상 터미널(PTY)을 요청합니다.
 * 2. 인터랙티브 셸 세션을 요청합니다.
 * 3. 루프를 돌며 다음 두 가지를 동시에 처리합니다:
 *    a. ssh 채널에서 들어오는 데이터를 읽어 화면(stdout)에 출력합니다.
 *    b. 사용자 키보드 입력(stdin)을 읽어 ssh 채널로 전송합니다.
 * 4. 채널이 닫히면 루프를 종료합니다.
 */
int interactive_shell(ssh_session session, ssh_channel channel, const char *initial_command) {
	int rc;
	struct termios orig_termios;
	int raw_mode_enabled = 0;
	int exit_code = 0;
	int stdin_is_open = 1;
	FILE *log_file = NULL;

	log_file = fopen("command_log.txt", "a");
	if (log_file) {
		time_t now = time(NULL);
		struct tm *t = localtime(&now);
		char time_buf[64];
		strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", t);
		fprintf(log_file, "\n--- New Session Started at %s ---\n", time_buf);
		fflush(log_file);
	}

	// Set up raw mode for the duration of this function
	if (isatty(STDIN_FILENO)) { //표준입력이 터미널일 때
		if (tcgetattr(STDIN_FILENO, &orig_termios) == 0) {
			struct termios raw = orig_termios;
			cfmakeraw(&raw);
			if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0) {
				raw_mode_enabled = 1;
			}
		}
	}

	// 1. Request a PTY (Pseudo-Terminal)
	rc = ssh_channel_request_pty(channel);
	if (rc != SSH_OK) {
		fprintf(stderr, "Failed to request PTY: %s\n", ssh_get_error(session));
		exit_code = -1;
		goto cleanup_shell;
	}

	// 2. Request an interactive shell
	rc = ssh_channel_request_shell(channel);
	if (rc != SSH_OK) {
		fprintf(stderr, "Failed to request shell: %s\n", ssh_get_error(session));
		exit_code = -1;
		goto cleanup_shell;
	}

	printf("[+] Interactive shell is ready. Press Ctrl+D on a new line to exit.\n");

	// If an initial command is provided, write it to the shell.
	if (initial_command != NULL) {
		ssh_channel_write(channel, initial_command, strlen(initial_command));
		// Send a newline to execute the command
		ssh_channel_write(channel, "\n", 1);
	}

	// 3. Interactive loop using select() for efficient I/O
	while (ssh_channel_is_open(channel) && !ssh_channel_is_eof(channel)) {
		fd_set fds;
		struct timeval tv;
		int maxfd;

		tv.tv_sec = 0;
		tv.tv_usec = 100000; // 100ms timeout

		int ssh_fd = ssh_get_fd(session);
		if (ssh_fd < 0) {
			exit_code = -1;
			break;
		}

		FD_ZERO(&fds);
		FD_SET(ssh_fd, &fds);
		maxfd = ssh_fd;

		if (stdin_is_open) {
			FD_SET(STDIN_FILENO, &fds);
			if (STDIN_FILENO > maxfd) {
				maxfd = STDIN_FILENO;
			}
		}

		rc = select(maxfd + 1, &fds, NULL, NULL, &tv);

		if (rc < 0) {
			perror("select()");
			break;
		}

		// 3b. Read from user stdin and write to remote shell
		if (stdin_is_open && FD_ISSET(STDIN_FILENO, &fds)) {
			char buffer[1024];
			int nbytes = read(STDIN_FILENO, buffer, sizeof(buffer));
			if (nbytes > 0) {
				if (log_file) {
					fwrite(buffer, 1, nbytes, log_file);
					fflush(log_file);
				}
				if (ssh_channel_write(channel, buffer, nbytes) == SSH_ERROR) {
					fprintf(stderr, "Error writing to channel: %s\n", ssh_get_error(session));
					break;
				}
			} else { // nbytes <= 0 means pipe closed, Ctrl+D, or read error
				stdin_is_open = 0;
				ssh_channel_send_eof(channel);
			}
		}

		// 3a. Read from remote shell and write to stdout
		if (FD_ISSET(ssh_fd, &fds)) {
			char buffer[1024];
			int nbytes;
			do {
				nbytes = ssh_channel_read_nonblocking(channel, buffer, sizeof(buffer), 0);
				if (nbytes > 0) {
					if (log_file) {
						fwrite(buffer, 1, nbytes, log_file);
						fflush(log_file);
					}
					if (write(STDOUT_FILENO, buffer, nbytes) != (unsigned int)nbytes) {
						fprintf(stderr, "Error writing to stdout.\n");
						exit_code = -1;
						goto cleanup_shell;
					}
				}
			} while (nbytes > 0);
		}
	}

cleanup_shell:
	if (log_file) {
		time_t now = time(NULL);
		struct tm *t = localtime(&now);
		char time_buf[64];
		strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", t);
		fprintf(log_file, "\n--- Session Ended at %s ---\n", time_buf);
		fclose(log_file);
	}
	// Restore terminal settings before leaving the function
	if (raw_mode_enabled) {
		tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
	}
	return exit_code;
}

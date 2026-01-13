# Bandit_Bot Flowchart

This document outlines the execution flow of the `bandit_bot.c` program.

```mermaid
graph TD
    subgraph main
        A[Start] --> B{Initialize SSH Session};
        B --> C{Set SSH Options\n(Host, Port, User)};
        C --> D{Connect to SSH Server};
        D -- OK --> E{Authenticate with Password};
        D -- Fail --> Z[Print Error & Cleanup];
        E -- Success --> F{Create SSH Channel};
        E -- Fail --> Z;
        F -- OK --> G{Open Channel Session};
        F -- Fail --> Z;
        G -- OK --> H{Check for Initial Command\n(Command-line argument)};
        G -- Fail --> Z;
        H --> I[Call interactive_shell];
    end

    subgraph interactive_shell
        J[Start interactive_shell] --> K{Open log file 'command_log.txt'};
        K --> L{Set Terminal to Raw Mode};
        L --> M{Request PTY};
        M -- OK --> N{Request Shell};
        M -- Fail --> X[Cleanup Shell & Return];
        N -- OK --> O{Initial Command Provided?};
        N -- Fail --> X;
        O -- Yes --> P[Write Command to Channel];
        O -- No --> Q;
        P --> Q[Start Interactive Loop];
        
        Q -- Channel Open --> R{Use select() to wait for I/O\n(stdin or SSH)};
        R -- stdin ready --> S[Read from stdin];
        S --> T[Write to SSH Channel & Log];
        T --> Q;
        S -- End of Input (Ctrl+D) --> U[Send EOF to Channel];
        U --> W;

        R -- SSH ready --> V[Read from SSH Channel];
        V --> W[Write to stdout & Log];
        W --> Q;

        Q -- Channel Closed/EOF --> X;
    end
    
    I --> J;
    X --> Y[main: Cleanup Session];
    Y --> End[End];
    Z --> Y;
```

## Flow Description

### `main` Function

1.  **Start**: The program begins execution.
2.  **Initialize SSH Session**: Creates a new SSH session object.
3.  **Set SSH Options**: Configures the connection details (target host, port, and username).
4.  **Connect to SSH Server**: Attempts to establish a connection to the specified server. If it fails, it prints an error and proceeds to the cleanup phase.
5.  **Authenticate**: Tries to authenticate with the server using a hardcoded password. If authentication fails, it prints an error and goes to cleanup.
6.  **Create SSH Channel**: If authentication is successful, it creates a new SSH channel.
7.  **Open Channel Session**: Opens the newly created channel.
8.  **Check for Initial Command**: Checks if the user provided a command as a command-line argument.
9.  **Call `interactive_shell`**: Transfers control to the `interactive_shell` function to handle the interactive session.

### `interactive_shell` Function

1.  **Start `interactive_shell`**: The function begins, receiving the active session and channel.
2.  **Open Log File**: Opens `command_log.txt` in append mode to log the session.
3.  **Set Terminal to Raw Mode**: Configures the local terminal to handle input character-by-character without buffering.
4.  **Request PTY**: Asks the remote server to allocate a pseudo-terminal.
5.  **Request Shell**: Asks the server to start a shell.
6.  **Initial Command?**: Checks if an initial command was passed from `main`. If so, it writes the command to the channel to be executed immediately.
7.  **Interactive Loop**: This is the core of the function.
    *   It uses the `select()` system call to monitor both the user's standard input (`stdin`) and the SSH channel for incoming data.
    *   **Input from `stdin`**: If the user types something, it's read, written to the remote SSH channel, and logged to the file. If the user signals the end of input (e.g., by pressing `Ctrl+D`), an EOF (End-Of-File) packet is sent to the remote channel.
    *   **Input from SSH**: If data arrives from the remote server, it's read, written to the user's standard output (`stdout`), and logged to the file.
8.  **Loop Exit**: The loop continues until the SSH channel is closed or an error occurs.
9.  **Cleanup Shell**: Restores the terminal to its original settings and closes the log file.
10. **Return to `main`**: Control returns to the `main` function.

### Final Cleanup

1.  **`main`: Cleanup Session**: The `main` function performs final cleanup by closing the SSH channel, disconnecting from the server, and freeing the session resources.
2.  **End**: The program terminates.

```
# What is HTTP-Public-Message-Protocol / HPMP?
The main idea of the project is to utilize HPMP inside of a controlled local network environment and to be able to open the HTTP interface via an IoT device.<br>
The concept is to understand in more detail how network connections work in low level.

## Installation & Compiling:

```console
git clone https://github.com/RodricBr/HTTP-Public-Message-Protocol
cd HTTP-Public-Message-Protocol/

# Compiling: 

gcc CLI-HPMP-Sender.c -o CLI-HPMP-Sender
gcc HPMP-Server.c -o HPMP-Server
```

## Executing:

> [!IMPORTANT]
> Server needs to be executed with super user privileges

- Initializing server which creates the HTTP-PMP Server:
```console
./HPMP-Server
```

- Sending messages via CLI:
```console
./CLI-HPMP-Sender -m "Message to send" <SERVER IP>
```

## Examples:
- Executing Server and showing log contents:
<img width="605" height="123" alt="image" src="https://github.com/user-attachments/assets/2ab70baa-96ff-467c-be51-8802d3a4c0e4" />
<img width="667" height="83" alt="image" src="https://github.com/user-attachments/assets/418e3721-8235-4a7a-ae94-f5a55c02caf7" />

- HTTP Sender:
<img width="653" height="566" alt="image" src="https://github.com/user-attachments/assets/c58bf931-ef46-4283-bbf9-51c61214899f" />

- Sending messages via CLI:
<img width="554" height="45" alt="image" src="https://github.com/user-attachments/assets/c29f9283-10de-4cf8-9fd9-40d9f7b046ed" />




# Security Policy

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 2.0.x   | :white_check_mark: |
| 1.x     | :x:                |

## Security Features

### Authentication
- **OTA Updates**: Password-protected using device-specific password (ESP chip ID)
- **WiFi Configuration Portal**: Unique password per device (based on chip ID)
- **MQTT**: Username/password authentication supported

### Data Protection
- **Plaintext Storage**: WiFi and MQTT credentials stored unencrypted in SPIFFS
- **No TLS**: MQTT communication is unencrypted (memory constraints)
- **Physical Security Required**: Device with physical access can extract credentials

### Input Validation
- MQTT payload size limits enforced (8 bytes)
- MQTT topic parsing validates zone numbers (1-7 only)
- Configuration validation prevents invalid MQTT port numbers
- Buffer overflow protection via snprintf/strlcpy

### Safety Features
- Zone runtime limits (2-hour maximum per zone)
- Configuration validation on load
- SPIFFS mount retry logic (3 attempts)

## Known Limitations

### Critical
1. **Plaintext Credential Storage**: Credentials stored unencrypted on device flash
   - **Mitigation**: Use dedicated IoT credentials with minimal privileges

2. **No MQTT TLS**: MQTT traffic is unencrypted
   - **Mitigation**: Deploy on isolated IoT VLAN, don't expose broker to internet

### Medium
3. **No Credential Rotation**: Credentials persist until manually changed
   - **Mitigation**: Periodically update via configuration portal

4. **Serial Console Access**: Physical access allows credential viewing
   - **Mitigation**: Disable DEBUG in production builds

## Threat Model

### Assets
- WiFi credentials
- MQTT broker credentials
- Zone control (water system access)
- Device firmware

### Attack Vectors

#### Network-Based Attacks
1. **Man-in-the-Middle** (MEDIUM risk)
   - MQTT traffic is unencrypted
   - Configuration portal uses HTTP
   - Mitigation: Network-level security (WPA3, VLAN isolation)

2. **Unauthorized OTA Upload** (LOW risk after v2.0)
   - Requires network access + password
   - Password based on chip ID (not guessable)
   - Mitigation: Unique passwords per device

#### Physical Access Attacks
1. **Credential Extraction** (HIGH risk)
   - SPIFFS can be dumped via USB
   - Credentials stored in plaintext JSON
   - Mitigation: Physical security, limited-privilege credentials

2. **Firmware Modification** (HIGH risk)
   - Serial access allows arbitrary firmware upload
   - Mitigation: Physical security, tamper-evident enclosure

#### MQTT Command Injection (LOW risk)
- Input validation prevents malformed commands
- Zone bounds checking prevents invalid zone access
- Mitigation: Additional MQTT ACL rules on broker

## Reporting a Vulnerability

Please report security vulnerabilities by creating a private issue on GitHub or emailing [your-email].

Include:
- Description of the vulnerability
- Steps to reproduce
- Potential impact
- Suggested mitigation (if any)

Response time: Within 48 hours for critical issues, 7 days for others.

## Security Best Practices for Users

1. **Network Isolation**
   - Deploy device on dedicated IoT VLAN
   - Firewall MQTT broker from internet access
   - Use WPA3 WiFi encryption

2. **Credential Management**
   - Create dedicated MQTT user with minimal privileges
   - Use strong passwords (20+ characters)
   - Don't reuse credentials from other systems

3. **Physical Security**
   - Install device in locked enclosure
   - Consider tamper-evident seals
   - Don't leave device accessible to untrusted persons

4. **Monitoring**
   - Monitor MQTT broker logs for unusual activity
   - Set up Home Assistant alerts for unexpected zone activations
   - Check device uptime regularly (unexpected reboots may indicate tampering)

5. **Updates**
   - Keep firmware up to date
   - Review changelog for security fixes
   - Test updates in non-production environment first

## Security Changelog

### Version 2.0.0 (2025-10-26)
- Added unique OTA passwords per device
- Added unique configuration portal passwords
- Added configuration validation
- Added zone runtime safety limits
- Fixed buffer overflow vulnerabilities
- Improved input validation

### Version 1.0.0
- Initial release
- Basic security features
- Known vulnerabilities (see v2.0.0 fixes)

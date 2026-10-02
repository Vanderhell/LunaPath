# Security

LunaPath guard modes are integrity and error-detection primitives. CRC16, CRC32, and the reference rolling guard are not cryptographic MACs and do not authenticate events against a malicious adversary. Use an external cryptographic construction when adversarial integrity is required.

Please report implementation security issues privately to the project maintainer before public disclosure. Do not include secrets or sensitive device data in reports.

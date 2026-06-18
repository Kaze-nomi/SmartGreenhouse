import 'package:flutter/material.dart';

import '../models.dart';
import '../state/greenhouse_controller.dart';

class SettingsScreen extends StatefulWidget {
  const SettingsScreen({
    super.key,
    required this.controller,
  });

  final GreenhouseController controller;

  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  late final TextEditingController _hostController;
  late final TextEditingController _portController;
  late final TextEditingController _usernameController;
  late final TextEditingController _passwordController;
  late final TextEditingController _clientIdController;
  late final TextEditingController _deviceIdController;

  @override
  void initState() {
    super.initState();
    final settings = widget.controller.settings;
    _hostController = TextEditingController(text: settings.host);
    _portController = TextEditingController(text: settings.port.toString());
    _usernameController = TextEditingController(text: settings.username);
    _passwordController = TextEditingController(text: settings.password);
    _clientIdController = TextEditingController(text: settings.clientId);
    _deviceIdController = TextEditingController(text: settings.deviceId);
    widget.controller.addListener(_handleControllerChanged);
  }

  @override
  void dispose() {
    widget.controller.removeListener(_handleControllerChanged);
    _hostController.dispose();
    _portController.dispose();
    _usernameController.dispose();
    _passwordController.dispose();
    _clientIdController.dispose();
    _deviceIdController.dispose();
    super.dispose();
  }

  void _handleControllerChanged() {
    if (mounted) setState(() {});
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Настройки MQTT')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          _ConnectionCard(controller: widget.controller),
          const SizedBox(height: 12),
          _field(_hostController, 'Адрес MQTT-сервера', Icons.dns),
          const SizedBox(height: 12),
          _field(
            _portController,
            'Порт',
            Icons.numbers,
            keyboardType: TextInputType.number,
          ),
          const SizedBox(height: 12),
          _field(_usernameController, 'Логин', Icons.person),
          const SizedBox(height: 12),
          _field(
            _passwordController,
            'Пароль',
            Icons.password,
            obscureText: true,
          ),
          const SizedBox(height: 12),
          _field(_clientIdController, 'ID клиента MQTT', Icons.badge),
          const SizedBox(height: 12),
          _field(_deviceIdController, 'ID устройства ESP32', Icons.memory),
          const SizedBox(height: 20),
          FilledButton.icon(
            onPressed: _save,
            icon: const Icon(Icons.save),
            label: const Text('Сохранить настройки'),
          ),
          const SizedBox(height: 8),
          OutlinedButton.icon(
            onPressed: () async {
              await _saveSettingsOnly();
              await widget.controller.disconnect();
              if (!mounted) return;
              await widget.controller.connect();
            },
            icon: const Icon(Icons.link),
            label: const Text('Переподключиться'),
          ),
        ],
      ),
    );
  }

  Widget _field(
    TextEditingController controller,
    String label,
    IconData icon, {
    bool obscureText = false,
    TextInputType? keyboardType,
  }) {
    return TextField(
      controller: controller,
      decoration: InputDecoration(
        labelText: label,
        prefixIcon: Icon(icon),
      ),
      obscureText: obscureText,
      keyboardType: keyboardType,
    );
  }

  Future<void> _save() async {
    await _saveSettingsOnly();
    if (!mounted) return;
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text('Настройки сохранены')),
    );
  }

  Future<void> _saveSettingsOnly() async {
    final next = MqttSettings(
      host: _hostController.text.trim(),
      port: int.tryParse(_portController.text.trim()) ?? 1883,
      username: _usernameController.text.trim(),
      password: _passwordController.text,
      clientId: _clientIdController.text.trim(),
      deviceId: _deviceIdController.text.trim(),
    );
    await widget.controller.updateSettings(next);
  }
}

class _ConnectionCard extends StatelessWidget {
  const _ConnectionCard({required this.controller});

  final GreenhouseController controller;

  @override
  Widget build(BuildContext context) {
    final status = controller.connectionStatus;
    final connected = status == ConnectionStatus.connected;
    final connecting = status == ConnectionStatus.connecting;
    final color = connected
        ? Colors.green.shade700
        : status == ConnectionStatus.error
            ? Colors.red.shade700
            : Colors.orange.shade700;

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Row(
          children: [
            Icon(connected ? Icons.cloud_done : Icons.cloud_off, color: color),
            const SizedBox(width: 10),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    _statusText(status),
                    style: Theme.of(context).textTheme.titleMedium,
                  ),
                  Text(
                    '${controller.settings.host}:${controller.settings.port} / ${controller.settings.deviceId}',
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                  ),
                ],
              ),
            ),
            const SizedBox(width: 10),
            FilledButton.tonalIcon(
              onPressed: connecting
                  ? null
                  : connected
                      ? controller.disconnect
                      : controller.connect,
              icon: Icon(connected ? Icons.link_off : Icons.link),
              label: Text(connected ? 'Отключить' : 'Подключить'),
            ),
          ],
        ),
      ),
    );
  }

  String _statusText(ConnectionStatus status) {
    return switch (status) {
      ConnectionStatus.connected => 'MQTT подключён',
      ConnectionStatus.connecting => 'Подключение...',
      ConnectionStatus.error => 'Ошибка MQTT',
      ConnectionStatus.disconnected => 'MQTT отключён',
    };
  }
}

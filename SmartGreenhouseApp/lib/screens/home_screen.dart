import 'package:flutter/material.dart';

import '../models.dart';
import '../state/greenhouse_controller.dart';
import '../widgets/greenhouse_plan.dart';
import 'automation_screen.dart';
import 'settings_screen.dart';

class HomeScreen extends StatefulWidget {
  const HomeScreen({
    super.key,
    required this.controller,
  });

  final GreenhouseController controller;

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  String? _shownError;

  @override
  void initState() {
    super.initState();
    widget.controller.addListener(_handleControllerChanged);
  }

  @override
  void dispose() {
    widget.controller.removeListener(_handleControllerChanged);
    super.dispose();
  }

  void _handleControllerChanged() {
    final error = widget.controller.state.lastError;
    if (error == null || error.isEmpty || error == _shownError || !mounted) {
      return;
    }

    _shownError = error;
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (!mounted) return;
      showDialog<void>(
        context: context,
        builder: (context) => AlertDialog(
          title: const Text('Ошибка'),
          content: Text(error),
          actions: [
            TextButton(
              onPressed: () => Navigator.of(context).pop(),
              child: const Text('Понятно'),
            ),
          ],
        ),
      );
    });
  }

  @override
  Widget build(BuildContext context) {
    final state = widget.controller.state;
    final connected =
        widget.controller.connectionStatus == ConnectionStatus.connected;

    return Scaffold(
      appBar: AppBar(
        title: const Text('Умная теплица'),
        actions: [
          IconButton(
            tooltip: 'Автоматика',
            onPressed: () => Navigator.of(context).push(
              MaterialPageRoute(
                builder: (_) => AutomationScreen(controller: widget.controller),
              ),
            ),
            icon: const Icon(Icons.tune),
          ),
          IconButton(
            tooltip: 'Настройки MQTT',
            onPressed: () => Navigator.of(context).push(
              MaterialPageRoute(
                builder: (_) => SettingsScreen(controller: widget.controller),
              ),
            ),
            icon: const Icon(Icons.settings),
          ),
        ],
      ),
      body: SafeArea(
        child: ListView(
          padding: const EdgeInsets.all(12),
          children: [
            GreenhousePlan(
              zones: state.zones,
              windows: state.windows,
              airSensors: state.airSensors,
              onZoneTap: (zone) => _showZoneSheet(context, zone, connected),
            ),
            const SizedBox(height: 12),
            _ActionPanel(controller: widget.controller, enabled: connected),
            const SizedBox(height: 28),
          ],
        ),
      ),
    );
  }

  void _showZoneSheet(BuildContext context, SoilZone zone, bool connected) {
    final nameController = TextEditingController(text: zone.name);
    final durationController = TextEditingController(text: '60');

    showModalBottomSheet<void>(
      context: context,
      isScrollControlled: true,
      useSafeArea: true,
      builder: (context) {
        return Padding(
          padding: EdgeInsets.only(
            left: 16,
            right: 16,
            top: 16,
            bottom: 16 + MediaQuery.of(context).viewInsets.bottom,
          ),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                'Грядка ${zone.index}',
                style: Theme.of(context).textTheme.titleLarge,
              ),
              const SizedBox(height: 12),
              TextField(
                controller: nameController,
                decoration: const InputDecoration(
                  labelText: 'Название грядки',
                  prefixIcon: Icon(Icons.edit),
                ),
                textInputAction: TextInputAction.done,
                onSubmitted: (_) {
                  widget.controller.renameZone(zone.index, nameController.text);
                  Navigator.of(context).pop();
                },
              ),
              const SizedBox(height: 12),
              TextField(
                controller: durationController,
                decoration: const InputDecoration(
                  labelText: 'Длительность полива, сек.',
                  prefixIcon: Icon(Icons.timer),
                ),
                keyboardType: TextInputType.number,
              ),
              const SizedBox(height: 16),
              Row(
                children: [
                  Expanded(
                    child: FilledButton.icon(
                      onPressed: connected
                          ? () {
                              final duration =
                                  int.tryParse(durationController.text) ?? 60;
                              widget.controller.renameZone(
                                zone.index,
                                nameController.text,
                              );
                              widget.controller.waterZone(
                                zone.index,
                                durationSec: duration,
                              );
                              Navigator.of(context).pop();
                            }
                          : null,
                      icon: const Icon(Icons.water_drop),
                      label: const Text('Полить'),
                    ),
                  ),
                  const SizedBox(width: 10),
                  Expanded(
                    child: OutlinedButton.icon(
                      onPressed: connected
                          ? () {
                              widget.controller.stopIrrigation(
                                zone: zone.index,
                              );
                              Navigator.of(context).pop();
                            }
                          : null,
                      icon: const Icon(Icons.stop),
                      label: const Text('Стоп'),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 8),
              SizedBox(
                width: double.infinity,
                child: TextButton.icon(
                  onPressed: () {
                    widget.controller
                        .renameZone(zone.index, nameController.text);
                    Navigator.of(context).pop();
                  },
                  icon: const Icon(Icons.save),
                  label: const Text('Сохранить название'),
                ),
              ),
            ],
          ),
        );
      },
    );
  }
}

class _ActionPanel extends StatefulWidget {
  const _ActionPanel({
    required this.controller,
    required this.enabled,
  });

  final GreenhouseController controller;
  final bool enabled;

  @override
  State<_ActionPanel> createState() => _ActionPanelState();
}

class _ActionPanelState extends State<_ActionPanel> {
  late final TextEditingController _waterDurationController;
  late final TextEditingController _ventDurationController;

  @override
  void initState() {
    super.initState();
    _waterDurationController = TextEditingController(text: '60');
    _ventDurationController = TextEditingController(text: '60');
  }

  @override
  void dispose() {
    _waterDurationController.dispose();
    _ventDurationController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final waterDuration =
        int.tryParse(_waterDurationController.text.trim()) ?? 60;
    final ventDuration =
        int.tryParse(_ventDurationController.text.trim()) ?? 60;

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              'Быстрое управление',
              style: Theme.of(context).textTheme.titleMedium,
            ),
            const SizedBox(height: 12),
            Row(
              children: [
                Expanded(
                  child: TextField(
                    controller: _waterDurationController,
                    decoration: const InputDecoration(
                      labelText: 'Полив, сек.',
                      prefixIcon: Icon(Icons.timer),
                    ),
                    keyboardType: TextInputType.number,
                    onChanged: (_) => setState(() {}),
                  ),
                ),
                const SizedBox(width: 10),
                Expanded(
                  child: TextField(
                    controller: _ventDurationController,
                    decoration: const InputDecoration(
                      labelText: 'Вент., сек.',
                      prefixIcon: Icon(Icons.timer),
                    ),
                    keyboardType: TextInputType.number,
                    onChanged: (_) => setState(() {}),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 12),
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: [
                FilledButton.icon(
                  onPressed: widget.enabled
                      ? () => widget.controller.waterAll(
                            zoneDurationSec: waterDuration,
                          )
                      : null,
                  icon: const Icon(Icons.water),
                  label: const Text('Полить всё'),
                ),
                OutlinedButton.icon(
                  onPressed: widget.enabled
                      ? () => widget.controller.stopIrrigation()
                      : null,
                  icon: const Icon(Icons.stop_circle),
                  label: const Text('Остановить полив'),
                ),
                OutlinedButton.icon(
                  onPressed: widget.enabled
                      ? () => widget.controller.setVentilation(
                            <int>[1],
                            'open',
                            durationSec: ventDuration,
                          )
                      : null,
                  icon: const Icon(Icons.north),
                  label: const Text('Открыть север'),
                ),
                OutlinedButton.icon(
                  onPressed: widget.enabled
                      ? () => widget.controller.setVentilation(
                            <int>[2],
                            'open',
                            durationSec: ventDuration,
                          )
                      : null,
                  icon: const Icon(Icons.south),
                  label: const Text('Открыть юг'),
                ),
                OutlinedButton.icon(
                  onPressed: widget.enabled
                      ? () => widget.controller.setVentilation(
                            <int>[1, 2],
                            'open',
                            durationSec: ventDuration,
                          )
                      : null,
                  icon: const Icon(Icons.air),
                  label: const Text('Открыть оба'),
                ),
                OutlinedButton.icon(
                  onPressed: widget.enabled
                      ? () => widget.controller.setVentilation(
                            <int>[1, 2],
                            'close',
                            durationSec: ventDuration,
                          )
                      : null,
                  icon: const Icon(Icons.horizontal_rule),
                  label: const Text('Закрыть оба'),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

import 'package:flutter/material.dart';

import '../models.dart';
import '../state/greenhouse_controller.dart';

class AutomationScreen extends StatefulWidget {
  const AutomationScreen({
    super.key,
    required this.controller,
  });

  final GreenhouseController controller;

  @override
  State<AutomationScreen> createState() => _AutomationScreenState();
}

class _AutomationScreenState extends State<AutomationScreen> {
  static const _modes = <String, String>{
    'manual': 'Вручную',
    'schedule': 'По расписанию',
    'sensor': 'По датчикам',
    'hybrid': 'Расписание + датчики',
  };

  late String _irrigationMode;
  late String _ventilationMode;
  late bool _safetyEnabled;
  late bool _blockOutsideAllowedTime;
  late bool _allowManualForce;
  late final TextEditingController _startController;
  late final TextEditingController _endController;
  late final TextEditingController _soilController;
  late final TextEditingController _humidityController;
  late final TextEditingController _tempController;
  late final TextEditingController _minMoistureController;
  late final TextEditingController _targetMoistureController;
  late final TextEditingController _maxWaterDurationController;
  late final TextEditingController _ventOpenTempController;
  late final TextEditingController _ventOpenHumidityController;
  late final TextEditingController _ventCloseTempController;
  late final TextEditingController _ventCloseHumidityController;
  late final TextEditingController _ventOpenDurationController;
  late final TextEditingController _ventCloseDurationController;

  final List<_WaterScheduleDraft> _waterSchedule = [
    _WaterScheduleDraft(time: '08:00'),
  ];
  final List<_VentScheduleDraft> _ventSchedule = [
    _VentScheduleDraft(time: '12:00'),
  ];

  @override
  void initState() {
    super.initState();
    final config = widget.controller.state.automation;
    _irrigationMode = config.irrigationMode;
    _ventilationMode = config.ventilationMode;
    _safetyEnabled = config.safetyEnabled;
    _blockOutsideAllowedTime = config.blockOutsideAllowedTime;
    _allowManualForce = config.allowManualForce;
    _startController = TextEditingController(text: config.safetyStart);
    _endController = TextEditingController(text: config.safetyEnd);
    _soilController =
        TextEditingController(text: config.blockIfSoilAbovePct.toString());
    _humidityController = TextEditingController(
      text: config.blockIfAirHumidityAbovePct.toString(),
    );
    _tempController =
        TextEditingController(text: config.blockIfAirTempBelowC.toString());
    _minMoistureController =
        TextEditingController(text: config.minMoisturePct.toString());
    _targetMoistureController =
        TextEditingController(text: config.targetMoisturePct.toString());
    _maxWaterDurationController =
        TextEditingController(text: config.maxIrrigationDurationSec.toString());
    _ventOpenTempController =
        TextEditingController(text: config.ventOpenTempC.toString());
    _ventOpenHumidityController =
        TextEditingController(text: config.ventOpenHumidityPct.toString());
    _ventCloseTempController =
        TextEditingController(text: config.ventCloseTempC.toString());
    _ventCloseHumidityController =
        TextEditingController(text: config.ventCloseHumidityPct.toString());
    _ventOpenDurationController =
        TextEditingController(text: config.ventOpenDurationSec.toString());
    _ventCloseDurationController =
        TextEditingController(text: config.ventCloseDurationSec.toString());
  }

  @override
  void dispose() {
    _startController.dispose();
    _endController.dispose();
    _soilController.dispose();
    _humidityController.dispose();
    _tempController.dispose();
    _minMoistureController.dispose();
    _targetMoistureController.dispose();
    _maxWaterDurationController.dispose();
    _ventOpenTempController.dispose();
    _ventOpenHumidityController.dispose();
    _ventCloseTempController.dispose();
    _ventCloseHumidityController.dispose();
    _ventOpenDurationController.dispose();
    _ventCloseDurationController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Автоматика')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          _modesCard(),
          const SizedBox(height: 12),
          _targetsCard(),
          const SizedBox(height: 12),
          _waterScheduleCard(),
          const SizedBox(height: 12),
          _ventScheduleCard(),
          const SizedBox(height: 12),
          _safetyCard(),
          const SizedBox(height: 24),
        ],
      ),
    );
  }

  Widget _modesCard() {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('Режим работы',
                style: Theme.of(context).textTheme.titleMedium),
            const SizedBox(height: 12),
            _modeSelector(
              label: 'Полив',
              value: _irrigationMode,
              onChanged: (value) => setState(() => _irrigationMode = value),
            ),
            const SizedBox(height: 12),
            _modeSelector(
              label: 'Проветривание',
              value: _ventilationMode,
              onChanged: (value) => setState(() => _ventilationMode = value),
            ),
            const SizedBox(height: 12),
            FilledButton.icon(
              onPressed: () {
                widget.controller.setModes(
                  irrigation: _irrigationMode,
                  ventilation: _ventilationMode,
                );
                _showInfo('Режимы отправлены на ESP32');
              },
              icon: const Icon(Icons.save),
              label: const Text('Сохранить режимы'),
            ),
          ],
        ),
      ),
    );
  }

  Widget _targetsCard() {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              'Пороги автоматики',
              style: Theme.of(context).textTheme.titleMedium,
            ),
            const SizedBox(height: 12),
            _field(_minMoistureController, 'Начать полив ниже, %'),
            const SizedBox(height: 12),
            _field(_targetMoistureController, 'Остановить полив при, %'),
            const SizedBox(height: 12),
            _field(
              _maxWaterDurationController,
              'Максимальный полив зоны, сек.',
            ),
            const Divider(height: 28),
            _field(
                _ventOpenTempController, 'Открыть, если температура выше, °C'),
            const SizedBox(height: 12),
            _field(
                _ventOpenHumidityController, 'Открыть, если влажность выше, %'),
            const SizedBox(height: 12),
            _field(
                _ventCloseTempController, 'Закрыть, если температура ниже, °C'),
            const SizedBox(height: 12),
            _field(_ventCloseHumidityController,
                'Закрыть, если влажность ниже, %'),
            const SizedBox(height: 12),
            Row(
              children: [
                Expanded(
                  child: _field(
                    _ventOpenDurationController,
                    'Открытие, сек.',
                  ),
                ),
                const SizedBox(width: 10),
                Expanded(
                  child: _field(
                    _ventCloseDurationController,
                    'Закрытие, сек.',
                  ),
                ),
              ],
            ),
            const SizedBox(height: 12),
            FilledButton.icon(
              onPressed: _sendTargets,
              icon: const Icon(Icons.track_changes),
              label: const Text('Сохранить пороги'),
            ),
          ],
        ),
      ),
    );
  }

  Widget _waterScheduleCard() {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              'Расписание полива',
              style: Theme.of(context).textTheme.titleMedium,
            ),
            const SizedBox(height: 12),
            for (var i = 0; i < _waterSchedule.length; i++) ...[
              _WaterScheduleRow(
                draft: _waterSchedule[i],
                onChanged: () => setState(() {}),
                onDelete: () => setState(() => _waterSchedule.removeAt(i)),
              ),
              const SizedBox(height: 10),
            ],
            Row(
              children: [
                Expanded(
                  child: OutlinedButton.icon(
                    onPressed: () => setState(
                      () => _waterSchedule.add(
                        _WaterScheduleDraft(time: '08:00'),
                      ),
                    ),
                    icon: const Icon(Icons.add),
                    label: const Text('Добавить время'),
                  ),
                ),
                const SizedBox(width: 10),
                Expanded(
                  child: FilledButton.icon(
                    onPressed: _sendWaterSchedule,
                    icon: const Icon(Icons.send),
                    label: const Text('Отправить'),
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _ventScheduleCard() {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              'Расписание проветривания',
              style: Theme.of(context).textTheme.titleMedium,
            ),
            const SizedBox(height: 12),
            for (var i = 0; i < _ventSchedule.length; i++) ...[
              _VentScheduleRow(
                draft: _ventSchedule[i],
                onChanged: () => setState(() {}),
                onDelete: () => setState(() => _ventSchedule.removeAt(i)),
              ),
              const SizedBox(height: 10),
            ],
            Row(
              children: [
                Expanded(
                  child: OutlinedButton.icon(
                    onPressed: () => setState(
                      () => _ventSchedule.add(
                        _VentScheduleDraft(time: '12:00'),
                      ),
                    ),
                    icon: const Icon(Icons.add),
                    label: const Text('Добавить время'),
                  ),
                ),
                const SizedBox(width: 10),
                Expanded(
                  child: FilledButton.icon(
                    onPressed: _sendVentSchedule,
                    icon: const Icon(Icons.send),
                    label: const Text('Отправить'),
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _safetyCard() {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('Защита полива',
                style: Theme.of(context).textTheme.titleMedium),
            SwitchListTile(
              contentPadding: EdgeInsets.zero,
              title: const Text('Защита включена'),
              value: _safetyEnabled,
              onChanged: (value) => setState(() => _safetyEnabled = value),
            ),
            SwitchListTile(
              contentPadding: EdgeInsets.zero,
              title: const Text('Запретить полив вне разрешённого времени'),
              value: _blockOutsideAllowedTime,
              onChanged: (value) =>
                  setState(() => _blockOutsideAllowedTime = value),
            ),
            Row(
              children: [
                Expanded(child: _field(_startController, 'Начало, МСК')),
                const SizedBox(width: 10),
                Expanded(child: _field(_endController, 'Конец, МСК')),
              ],
            ),
            const SizedBox(height: 12),
            _field(_soilController, 'Не поливать, если почва влажнее, %'),
            const SizedBox(height: 12),
            _field(_humidityController, 'Не поливать, если воздух влажнее, %'),
            const SizedBox(height: 12),
            _field(_tempController, 'Не поливать, если температура ниже, °C'),
            SwitchListTile(
              contentPadding: EdgeInsets.zero,
              title: const Text('Разрешить ручной обход защиты'),
              value: _allowManualForce,
              onChanged: (value) => setState(() => _allowManualForce = value),
            ),
            FilledButton.icon(
              onPressed: _sendSafety,
              icon: const Icon(Icons.shield),
              label: const Text('Сохранить защиту'),
            ),
          ],
        ),
      ),
    );
  }

  Widget _modeSelector({
    required String label,
    required String value,
    required ValueChanged<String> onChanged,
  }) {
    return DropdownButtonFormField<String>(
      initialValue: _modes.containsKey(value) ? value : 'manual',
      decoration: InputDecoration(labelText: label),
      items: _modes.entries
          .map((entry) => DropdownMenuItem(
                value: entry.key,
                child: Text(entry.value),
              ))
          .toList(),
      onChanged: (value) {
        if (value != null) onChanged(value);
      },
    );
  }

  Widget _field(
    TextEditingController controller,
    String label, {
    TextInputType keyboardType = TextInputType.number,
  }) {
    return TextField(
      controller: controller,
      decoration: InputDecoration(labelText: label),
      keyboardType: keyboardType,
    );
  }

  AutomationConfig _configFromFields() {
    return AutomationConfig(
      irrigationMode: _irrigationMode,
      ventilationMode: _ventilationMode,
      safetyEnabled: _safetyEnabled,
      blockOutsideAllowedTime: _blockOutsideAllowedTime,
      safetyStart: _startController.text.trim(),
      safetyEnd: _endController.text.trim(),
      blockIfSoilAbovePct: int.tryParse(_soilController.text.trim()) ?? 78,
      blockIfAirHumidityAbovePct:
          int.tryParse(_humidityController.text.trim()) ?? 88,
      blockIfAirTempBelowC: int.tryParse(_tempController.text.trim()) ?? 14,
      allowManualForce: _allowManualForce,
      minMoisturePct: int.tryParse(_minMoistureController.text.trim()) ?? 50,
      targetMoisturePct:
          int.tryParse(_targetMoistureController.text.trim()) ?? 62,
      maxIrrigationDurationSec:
          int.tryParse(_maxWaterDurationController.text.trim()) ?? 60,
      ventOpenTempC: int.tryParse(_ventOpenTempController.text.trim()) ?? 28,
      ventOpenHumidityPct:
          int.tryParse(_ventOpenHumidityController.text.trim()) ?? 78,
      ventCloseTempC: int.tryParse(_ventCloseTempController.text.trim()) ?? 24,
      ventCloseHumidityPct:
          int.tryParse(_ventCloseHumidityController.text.trim()) ?? 68,
      ventOpenDurationSec:
          int.tryParse(_ventOpenDurationController.text.trim()) ?? 60,
      ventCloseDurationSec:
          int.tryParse(_ventCloseDurationController.text.trim()) ?? 60,
    );
  }

  void _sendTargets() {
    final config = _configFromFields();
    widget.controller.updateIrrigationAutomation(config);
    widget.controller.updateVentilationAutomation(config);
    _showInfo('Пороги отправлены на ESP32');
  }

  void _sendSafety() {
    widget.controller.updateSafety(_configFromFields());
    _showInfo('Настройки защиты отправлены на ESP32');
  }

  void _sendWaterSchedule() {
    widget.controller.setIrrigationSchedule(
      _waterSchedule
          .where((item) => _isValidTime(item.time))
          .map(
            (item) => IrrigationScheduleItem(
              time: item.time,
              enabled: item.enabled,
              allZones: item.allZones,
              zones: item.selectedZones.toList()..sort(),
              durationSec: item.durationSec,
            ),
          )
          .toList(),
    );
    _showInfo('Расписание полива отправлено на ESP32');
  }

  void _sendVentSchedule() {
    widget.controller.setVentilationSchedule(
      _ventSchedule
          .where((item) => _isValidTime(item.time))
          .map(
            (item) => VentilationScheduleItem(
              time: item.time,
              enabled: item.enabled,
              windows: item.selectedWindows.toList()..sort(),
              action: item.action,
              durationSec: item.durationSec,
            ),
          )
          .toList(),
    );
    _showInfo('Расписание проветривания отправлено на ESP32');
  }

  bool _isValidTime(String value) {
    return RegExp(r'^([01]\d|2[0-3]):[0-5]\d$').hasMatch(value.trim());
  }

  void _showInfo(String text) {
    ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(text)));
  }
}

class _WaterScheduleDraft {
  _WaterScheduleDraft({required this.time})
      : selectedZones = {1, 2, 3, 4, 5, 6};

  String time;
  bool enabled = true;
  bool allZones = true;
  int durationSec = 60;
  final Set<int> selectedZones;
}

class _VentScheduleDraft {
  _VentScheduleDraft({required this.time}) : selectedWindows = {1, 2};

  String time;
  bool enabled = true;
  String action = 'open';
  int durationSec = 60;
  final Set<int> selectedWindows;
}

class _WaterScheduleRow extends StatelessWidget {
  const _WaterScheduleRow({
    required this.draft,
    required this.onChanged,
    required this.onDelete,
  });

  final _WaterScheduleDraft draft;
  final VoidCallback onChanged;
  final VoidCallback onDelete;

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      decoration: BoxDecoration(
        border: Border.all(color: const Color(0xFFD9E3D8)),
        borderRadius: BorderRadius.circular(8),
      ),
      child: Padding(
        padding: const EdgeInsets.all(10),
        child: Column(
          children: [
            Row(
              children: [
                Expanded(
                  child: TextFormField(
                    initialValue: draft.time,
                    decoration: const InputDecoration(labelText: 'Время'),
                    keyboardType: TextInputType.datetime,
                    onChanged: (value) => draft.time = value.trim(),
                  ),
                ),
                const SizedBox(width: 10),
                Expanded(
                  child: TextFormField(
                    initialValue: draft.durationSec.toString(),
                    decoration: const InputDecoration(labelText: 'Сек.'),
                    keyboardType: TextInputType.number,
                    onChanged: (value) =>
                        draft.durationSec = int.tryParse(value) ?? 60,
                  ),
                ),
                IconButton(
                  tooltip: 'Удалить',
                  onPressed: onDelete,
                  icon: const Icon(Icons.delete_outline),
                ),
              ],
            ),
            SwitchListTile(
              contentPadding: EdgeInsets.zero,
              title: const Text('Включено'),
              value: draft.enabled,
              onChanged: (value) {
                draft.enabled = value;
                onChanged();
              },
            ),
            SwitchListTile(
              contentPadding: EdgeInsets.zero,
              title: const Text('Поливать все грядки'),
              value: draft.allZones,
              onChanged: (value) {
                draft.allZones = value;
                onChanged();
              },
            ),
            if (!draft.allZones)
              Wrap(
                spacing: 6,
                children: [
                  for (var zone = 1; zone <= 6; zone++)
                    FilterChip(
                      label: Text('$zone'),
                      selected: draft.selectedZones.contains(zone),
                      onSelected: (selected) {
                        if (selected) {
                          draft.selectedZones.add(zone);
                        } else if (draft.selectedZones.length > 1) {
                          draft.selectedZones.remove(zone);
                        }
                        onChanged();
                      },
                    ),
                ],
              ),
          ],
        ),
      ),
    );
  }
}

class _VentScheduleRow extends StatelessWidget {
  const _VentScheduleRow({
    required this.draft,
    required this.onChanged,
    required this.onDelete,
  });

  final _VentScheduleDraft draft;
  final VoidCallback onChanged;
  final VoidCallback onDelete;

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      decoration: BoxDecoration(
        border: Border.all(color: const Color(0xFFD9E3D8)),
        borderRadius: BorderRadius.circular(8),
      ),
      child: Padding(
        padding: const EdgeInsets.all(10),
        child: Column(
          children: [
            Row(
              children: [
                Expanded(
                  child: TextFormField(
                    initialValue: draft.time,
                    decoration: const InputDecoration(labelText: 'Время'),
                    keyboardType: TextInputType.datetime,
                    onChanged: (value) => draft.time = value.trim(),
                  ),
                ),
                const SizedBox(width: 10),
                Expanded(
                  child: TextFormField(
                    initialValue: draft.durationSec.toString(),
                    decoration: const InputDecoration(labelText: 'Сек.'),
                    keyboardType: TextInputType.number,
                    onChanged: (value) =>
                        draft.durationSec = int.tryParse(value) ?? 60,
                  ),
                ),
                IconButton(
                  tooltip: 'Удалить',
                  onPressed: onDelete,
                  icon: const Icon(Icons.delete_outline),
                ),
              ],
            ),
            SwitchListTile(
              contentPadding: EdgeInsets.zero,
              title: const Text('Включено'),
              value: draft.enabled,
              onChanged: (value) {
                draft.enabled = value;
                onChanged();
              },
            ),
            DropdownButtonFormField<String>(
              initialValue: draft.action,
              decoration: const InputDecoration(labelText: 'Действие'),
              items: const [
                DropdownMenuItem(value: 'open', child: Text('Открыть')),
                DropdownMenuItem(value: 'close', child: Text('Закрыть')),
              ],
              onChanged: (value) {
                if (value == null) return;
                draft.action = value;
                onChanged();
              },
            ),
            const SizedBox(height: 8),
            Wrap(
              spacing: 6,
              children: [
                FilterChip(
                  label: const Text('Север'),
                  selected: draft.selectedWindows.contains(1),
                  onSelected: (selected) {
                    _toggleWindow(1, selected);
                    onChanged();
                  },
                ),
                FilterChip(
                  label: const Text('Юг'),
                  selected: draft.selectedWindows.contains(2),
                  onSelected: (selected) {
                    _toggleWindow(2, selected);
                    onChanged();
                  },
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  void _toggleWindow(int window, bool selected) {
    if (selected) {
      draft.selectedWindows.add(window);
    } else if (draft.selectedWindows.length > 1) {
      draft.selectedWindows.remove(window);
    }
  }
}

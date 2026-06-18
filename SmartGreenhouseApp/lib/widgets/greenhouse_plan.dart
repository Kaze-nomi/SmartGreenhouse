import 'package:flutter/material.dart';

import '../models.dart';

class GreenhousePlan extends StatelessWidget {
  const GreenhousePlan({
    super.key,
    required this.zones,
    required this.windows,
    required this.airSensors,
    required this.onZoneTap,
  });

  final List<SoilZone> zones;
  final List<WindowStatus> windows;
  final List<AirSensor> airSensors;
  final ValueChanged<SoilZone> onZoneTap;

  @override
  Widget build(BuildContext context) {
    final northZones = zones.take(3).toList();
    final southZones = zones.skip(3).take(3).toList();

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('Теплица', style: Theme.of(context).textTheme.titleMedium),
            const SizedBox(height: 10),
            _VentStrip(
              title: 'Северное окно',
              status: windows.isNotEmpty ? windows.first.state : 'unknown',
              remainingSec: windows.isNotEmpty ? windows.first.remainingSec : 0,
            ),
            const SizedBox(height: 10),
            _SectionHeader(
              title: 'Северная секция',
              icon: Icons.north,
              sensor: _sensorById('air1'),
            ),
            const SizedBox(height: 8),
            _ZoneRow(zones: northZones, onTap: onZoneTap),
            const SizedBox(height: 14),
            _SectionHeader(
              title: 'Южная секция',
              icon: Icons.south,
              sensor: _sensorById('air2'),
            ),
            const SizedBox(height: 8),
            _ZoneRow(zones: southZones, onTap: onZoneTap),
            const SizedBox(height: 10),
            _VentStrip(
              title: 'Южное окно',
              status: windows.length > 1 ? windows[1].state : 'unknown',
              remainingSec: windows.length > 1 ? windows[1].remainingSec : 0,
            ),
          ],
        ),
      ),
    );
  }

  AirSensor _sensorById(String id) {
    for (final sensor in airSensors) {
      if (sensor.id == id) return sensor;
    }
    return AirSensor(id: id);
  }
}

class _SectionHeader extends StatelessWidget {
  const _SectionHeader({
    required this.title,
    required this.icon,
    required this.sensor,
  });

  final String title;
  final IconData icon;
  final AirSensor sensor;

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      decoration: BoxDecoration(
        color: const Color(0xFFF3F7F1),
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: const Color(0xFFD9E3D8)),
      ),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
        child: Row(
          children: [
            Icon(icon, size: 20, color: const Color(0xFF2F7D5C)),
            const SizedBox(width: 8),
            Expanded(
              child: Text(
                title,
                style: Theme.of(context).textTheme.titleSmall?.copyWith(
                      fontWeight: FontWeight.w700,
                    ),
              ),
            ),
            _AirValue(
              icon: Icons.thermostat,
              value: sensor.temperatureC == null
                  ? '--'
                  : '${sensor.temperatureC!.toStringAsFixed(1)} °C',
            ),
            const SizedBox(width: 10),
            _AirValue(
              icon: Icons.water_drop,
              value: sensor.humidityPct == null
                  ? '--'
                  : '${sensor.humidityPct!.toStringAsFixed(0)} %',
            ),
          ],
        ),
      ),
    );
  }
}

class _AirValue extends StatelessWidget {
  const _AirValue({
    required this.icon,
    required this.value,
  });

  final IconData icon;
  final String value;

  @override
  Widget build(BuildContext context) {
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Icon(icon, size: 16, color: Colors.black54),
        const SizedBox(width: 3),
        Text(
          value,
          style: Theme.of(context).textTheme.labelLarge?.copyWith(
                fontWeight: FontWeight.w700,
              ),
        ),
      ],
    );
  }
}

class _ZoneRow extends StatelessWidget {
  const _ZoneRow({
    required this.zones,
    required this.onTap,
  });

  final List<SoilZone> zones;
  final ValueChanged<SoilZone> onTap;

  @override
  Widget build(BuildContext context) {
    return Row(
      children: zones
          .map((zone) => Expanded(
                child: Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 4),
                  child: _Bed(zone: zone, onTap: () => onTap(zone)),
                ),
              ))
          .toList(),
    );
  }
}

class _Bed extends StatelessWidget {
  const _Bed({
    required this.zone,
    required this.onTap,
  });

  final SoilZone zone;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final moisture = zone.moisturePct;
    final fillColor = _moistureColor(moisture);
    final theme = Theme.of(context);

    return Material(
      color: fillColor,
      borderRadius: BorderRadius.circular(8),
      child: InkWell(
        borderRadius: BorderRadius.circular(8),
        onTap: onTap,
        child: Container(
          height: 168,
          padding: const EdgeInsets.all(10),
          decoration: BoxDecoration(
            borderRadius: BorderRadius.circular(8),
            border: Border.all(
              color: zone.valveOn ? const Color(0xFF0D47A1) : Colors.black26,
              width: zone.valveOn ? 3 : 1,
            ),
          ),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                children: [
                  Text(
                    '${zone.index}',
                    style: theme.textTheme.titleMedium?.copyWith(
                      fontWeight: FontWeight.w800,
                    ),
                  ),
                  const Spacer(),
                  Icon(
                    zone.valveOn ? Icons.water_drop : Icons.water_drop_outlined,
                    size: 20,
                    color: zone.valveOn ? Colors.blue.shade800 : Colors.black54,
                  ),
                ],
              ),
              const Spacer(),
              Text(
                zone.name,
                maxLines: 2,
                overflow: TextOverflow.ellipsis,
                style: theme.textTheme.titleSmall?.copyWith(
                  fontWeight: FontWeight.w700,
                ),
              ),
              const SizedBox(height: 8),
              SizedBox(
                height: 34,
                child: FittedBox(
                  fit: BoxFit.scaleDown,
                  alignment: Alignment.centerLeft,
                  child: Text(
                    moisture == null ? 'Нет данных' : '$moisture%',
                    maxLines: 1,
                    style: theme.textTheme.titleLarge?.copyWith(
                      fontWeight: FontWeight.w800,
                    ),
                  ),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  Color _moistureColor(int? moisture) {
    if (moisture == null) return const Color(0xFFE1E6DD);
    if (moisture < 50) return const Color(0xFFF3C995);
    if (moisture > 78) return const Color(0xFF9FCBF2);
    return const Color(0xFFA8D7A1);
  }
}

class _VentStrip extends StatelessWidget {
  const _VentStrip({
    required this.title,
    required this.status,
    required this.remainingSec,
  });

  final String title;
  final String status;
  final int remainingSec;

  @override
  Widget build(BuildContext context) {
    final active = status == 'open' || status == 'opening';
    final text = _windowStatus(status);
    return Container(
      constraints: const BoxConstraints(minHeight: 42),
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
      decoration: BoxDecoration(
        color: active ? const Color(0xFFB7D7EF) : const Color(0xFFD8E0D3),
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: Colors.black26),
      ),
      child: Row(
        children: [
          Icon(active ? Icons.air : Icons.horizontal_rule, size: 18),
          const SizedBox(width: 8),
          Expanded(
            child: Text(
              remainingSec > 0
                  ? '$title: $text, $remainingSec сек.'
                  : '$title: $text',
              maxLines: 2,
              overflow: TextOverflow.ellipsis,
            ),
          ),
        ],
      ),
    );
  }

  String _windowStatus(String status) {
    return switch (status) {
      'open' => 'открыто',
      'closed' => 'закрыто',
      'opening' => 'открывается',
      'closing' => 'закрывается',
      'stopped' => 'остановлено',
      'unknown' => 'нет данных',
      _ => status,
    };
  }
}

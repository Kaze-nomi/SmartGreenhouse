import 'package:flutter/material.dart';

import 'screens/home_screen.dart';
import 'state/greenhouse_controller.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  final controller = GreenhouseController();
  await controller.initialize();
  runApp(GreenhouseApp(controller: controller));
}

class GreenhouseApp extends StatelessWidget {
  const GreenhouseApp({
    super.key,
    required this.controller,
  });

  final GreenhouseController controller;

  @override
  Widget build(BuildContext context) {
    return AnimatedBuilder(
      animation: controller,
      builder: (context, _) {
        return MaterialApp(
          title: 'Умная теплица',
          debugShowCheckedModeBanner: false,
          theme: ThemeData(
            useMaterial3: true,
            colorScheme: ColorScheme.fromSeed(
              seedColor: const Color(0xFF2F7D5C),
              brightness: Brightness.light,
            ),
            scaffoldBackgroundColor: const Color(0xFFF5F7F3),
            appBarTheme: const AppBarTheme(
              centerTitle: false,
              backgroundColor: Color(0xFFF5F7F3),
              foregroundColor: Color(0xFF173D2E),
            ),
            cardTheme: CardThemeData(
              elevation: 0,
              color: Colors.white,
              shape: RoundedRectangleBorder(
                borderRadius: BorderRadius.circular(8),
                side: const BorderSide(color: Color(0xFFE0E7DE)),
              ),
            ),
            inputDecorationTheme: const InputDecorationTheme(
              border: OutlineInputBorder(),
            ),
          ),
          home: HomeScreen(controller: controller),
        );
      },
    );
  }
}

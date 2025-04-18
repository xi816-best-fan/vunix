# 🖥️ Vunix - Учебная операционная система

**Минималистичная UNIX-подобная ОС для образовательных целей**, написанная на C. Проект был создан для изучения принципов работы осей. На данный момент времени закрыт из-за того что будет переписан.

## 🌟 Особенности

- 🏗️ **Микроядро** с поддержкой процессов, задач и виртуальной памяти
- 📂 **Виртуальная файловая система** (VFS) с базовыми операциями
- 🔍 **Отладочные инструменты**: ~~упрощенный `ptrace`~~, системный логгер
- 🛠️ **Системные вызовы**: ~~`fork`, `execve`, `read/write`, `ioctl`~~ Нету
- 🧩 **Драйверы устройств**: терминал, таймер, псевдо-диск

## 🚀 Быстрый старт

### Сборка (Linux/macOS)

```bash
# Клонировать репо
git clone https://github.com/xi816-best-fan/vunix.git
cd vunix

# Собрать проект
./make.sh

# Запустить загрузчик
./tinyboot
```
## 🤝 Участие в проекте
Приветствуются:
Исправления ошибок
Реализация новых системных вызовов
Улучшение документации
Создание тестов

Правила:
1. Форкните репозиторий
2. Создайте ветку (git checkout -b feature/your-feature)
3. Коммитьте (git commit -am 'Add awesome feature')
4. Пушите (git push origin feature/your-feature)
5. Создайте Pull Request

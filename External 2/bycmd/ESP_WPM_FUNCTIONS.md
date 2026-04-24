# ESP & Rage Functions - WPM Usage Documentation

## ⚠️ ФУНКЦИИ ИСПОЛЬЗУЮЩИЕ WPM (ЗАПИСЬ В ПАМЯТЬ)

### 🔴 ESP ФУНКЦИИ (Только чтение - БЕЗОПАСНО)
ВСЕ ESP функции используют **ТОЛЬКО RPM** (чтение памяти) и НЕ используют WPM:
- ✅ Box ESP
- ✅ Name ESP
- ✅ Health/Armor ESP
- ✅ Distance ESP
- ✅ Skeleton ESP
- ✅ Weapon ESP (text/icon)
- ✅ Bomb ESP + Timer
- ✅ Grenade Type ESP + Trajectory
- ✅ Dropped Weapons ESP
- ✅ Player Trails
- ✅ Footsteps ESP
- ✅ Bullet Tracers
- ✅ Death Silhouette
- ✅ Ping ESP
- ✅ Platform ESP
- ✅ K/D/A ESP
- ✅ Money ESP
- ✅ Visibility Check (is_visible)

**ESP = 100% БЕЗОПАСНО, только чтение!**

---

### 🟠 CHAMS / ВИЗУАЛЬНЫЕ МОДИФИКАЦИИ (Используют WPM)
| Функция | Что делает | Риск |
|---------|------------|------|
| `chams_enabled` | Изменяет цвет/материал моделей игроков | ⚠️ Средний |
| `chams_model_mode` | Рендерит модели игроков сквозь стены | ⚠️ Средний |

**Chams пишут в память игровых объектов!**

---

### 🔴 RAGE ФУНКЦИИ (Используют WPM - ВЫСОКИЙ РИСК)

#### Combat / Weapon
| Функция | Адрес записи | Описание |
|---------|--------------|----------|
| `no_recoil` | GunController + 0x220 | Обнуляет отдачу |
| `no_spread` | GunController + 0x220 | Обнуляет разброс |
| `recoil_control` | GunController + 0x108 | Контроль отдачи |
| `infinity_ammo` | GunController + 0x110/0x114 | Бесконечные патроны |
| `rapid_fire` | GunController + 0x100 | Скорострельность |
| `fire_rate` | GunController + 0x100 | Скорость огня |
| `wallshot` | WeaponController + 0xA8 | Пробитие стен |
| `one_hit_kill` | GunController + 0x140 | Урон 9999 |
| `fast_knife` | WeaponController + 0x100 | Скорость ножа |

#### Movement
| Функция | Адрес записи | Описание |
|---------|--------------|----------|
| `air_jump` | JumpParameters + 0x10/0x60 | Прыжки в воздухе |
| `bunnyhop` | JumpParameters + 0x10/0x60 | Авто-прыжки |
| `auto_strafe` | TranslationParameters + 0x2C/0x30 | Стрейфы |

#### Visual / Camera
| Функция | Адрес записи | Описание |
|---------|--------------|----------|
| `aspect_ratio` | Camera + 0x4f0 | Соотношение сторон |
| `camera_fov` | Camera + 0x180 | FOV камеры |
| `hands_position` | ArmsAnimationController + ? | Позиция рук |
| `viewmodel_changer` | ArmsAnimationController + 0xE8 | ViewModel |
| `sky_color` | RenderSettings + ? | Цвет неба |

#### Game / Network
| Функция | Адрес записи | Описание |
|---------|--------------|----------|
| `anti_clumsy` | PhotonView + 0x50 | Фикс лагов |
| `clumsy` | PhotonView + 0x50 | Имитация лага |
| `money_hack` | PropertiesRegistry | Деньги |
| `cancel_match` | PhotonNetwork | Отмена матча |
| `set_score` | PhotonPlayer | Скор |
| `set_kills` | PhotonPlayer | Киллы |
| `set_assists` | PhotonPlayer | Ассисты |
| `set_deaths` | PhotonPlayer | Смерти |
| `autowin` | PhotonNetwork | Авто-победа |

#### Grenade
| Функция | Адрес записи | Описание |
|---------|--------------|----------|
| `fast_detonation` | GrenadeController | Быстрый взрыв |
| `no_bomb_damage` | GameModeSettings | Нет урона от бомбы |
| `infinity_grenades` | GunController + 0x110/0x114 | Бесконечные гранаты |

#### Other
| Функция | Адрес записи | Описание |
|---------|--------------|----------|
| `big_head` | PlayerController/Transform | Большие головы |
| `buy_anywhere` | BuyController | Покупка везде |
| `infinity_buy` | BuyController | Бесконечные покупки |
| `host_indicator` | PhotonNetwork | Проверка хоста |

---

## 📊 РЕКОМЕНДАЦИИ ПО БЕЗОПАСНОСТИ

### 🟢 Безопасно (только RPM)
- Весь ESP (Box, Name, Skeleton, Health, etc.)
- Таймеры (Bomb, Grenade)
- Траектории (Grenade, Bullet)
- Индикаторы (Ping, Platform, KDA, Money)

### 🟡 Средний риск (WPM в визуальные объекты)
- Chams (цвета моделей)

### 🔴 Высокий риск (WPM в геймплей)
- No recoil / No spread
- Infinity ammo
- Wallshot
- One hit kill
- Bunnyhop / Air jump
- Money hack
- Score/kills manipulation

---

## 💡 СОВЕТ
**Для минимального риска используйте ТОЛЬКО ESP функции!**
Все ESP функции только читают память и не изменяют игровые данные.

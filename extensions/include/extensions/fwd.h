#ifndef EXTENSIONS_FWD_H
#define EXTENSIONS_FWD_H

namespace Extensions
{
class FirstPersonCameraExt;
class MultiplayerExt;
class ThirdPersonCameraExt;
namespace Common
{
class CharacterCloner;
}
namespace FirstPersonCamera
{
class Controller;
}
namespace ThirdPersonCamera
{
class Controller;
class OrbitCamera;
} // namespace ThirdPersonCamera
} // namespace Extensions

namespace Multiplayer
{
class NetworkManager;
class WorldStateSync;
namespace Animation
{
class AudioPlayer;
class Catalog;
class Controller;
} // namespace Animation
} // namespace Multiplayer

#endif // EXTENSIONS_FWD_H

#include "game/animation/UnitAnimationManifest.hpp"

#include <spine/Animation.h>
#include <spine/SkeletonData.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace autochess::game
{
    namespace
    {
        std::string lowerAscii(const std::string& value)
        {
            std::string result = value;
            std::transform(
                result.begin(),
                result.end(),
                result.begin(),
                [](const unsigned char character) {
                    return static_cast<char>(std::tolower(character));
                });
            return result;
        }

        // 素材中部分动作带单字符 v/t 导出后缀，例如 MoveT、DieV。
        // 只有去除后能形成已知动作名时才剥离，不能误伤 Start 或
        // Skill_1_Start 这类名称本身就以 t 结尾的动作。
        std::string normalize(const std::string& value)
        {
            std::string result = lowerAscii(value);
            if (result.size() <= 1
                || (result.back() != 'v' && result.back() != 't'))
            {
                return result;
            }

            const std::string candidate = result.substr(0, result.size() - 1);
            static const std::vector<std::string> actionPrefixes{
                "relax", "move", "start", "attack", "die",
                "interact", "sit", "sleep"};
            const bool knownAction = std::any_of(
                actionPrefixes.begin(),
                actionPrefixes.end(),
                [&candidate](const std::string& prefix)
                {
                    return candidate == prefix
                        || (candidate.size() > prefix.size()
                            && candidate.compare(0, prefix.size(), prefix) == 0
                            && candidate[prefix.size()] == '_');
                });
            return knownAction ? candidate : result;
        }

        bool startsWith(const std::string& value, const std::string& prefix)
        {
            return value.size() >= prefix.size()
                && value.compare(0, prefix.size(), prefix) == 0;
        }

        int variantRank(
            const std::string& normalized,
            const std::string& exact)
        {
            if (normalized == exact)
            {
                return 0;
            }
            if (normalized.find(exact + "_a") == 0
                || normalized.find(exact + "_a_") == 0)
            {
                return 1;
            }
            return 2;
        }

        void chooseBest(
            std::string& output,
            int& outputRank,
            const std::string& candidate,
            const int candidateRank)
        {
            if (candidateRank < outputRank
                || (candidateRank == outputRank
                    && (output.empty() || candidate < output)))
            {
                output = candidate;
                outputRank = candidateRank;
            }
        }
    }

    UnitAnimationManifest UnitAnimationManifest::fromSkeletonData(
        spine::SkeletonData& skeletonData)
    {
        UnitAnimationManifest manifest;
        int relaxRank = 100;
        int moveRank = 100;
        int startRank = 100;
        int attackRank = 100;
        int attackBeginRank = 100;
        int attackEndRank = 100;
        int dieRank = 100;
        std::string skillAttackBegin;
        std::string skillAttackLoop;
        std::string skillAttackEnd;

        for (std::size_t index = 0;
             index < skeletonData.getAnimations().size();
             ++index)
        {
            spine::Animation* animation = skeletonData.getAnimations()[index];
            if (animation == nullptr || animation->getName().buffer() == nullptr)
            {
                continue;
            }

            const std::string originalName(animation->getName().buffer());
            const std::string normalizedName = normalize(originalName);

            if (normalizedName == "relax")
            {
                chooseBest(manifest.relax, relaxRank, originalName, 0);
            }
            else if (normalizedName == "move")
            {
                chooseBest(manifest.move, moveRank, originalName, 0);
            }
            else if (normalizedName == "start"
                     || startsWith(normalizedName, "start_"))
            {
                chooseBest(
                    manifest.start,
                    startRank,
                    originalName,
                    variantRank(normalizedName, "start"));
            }
            else if (normalizedName == "die"
                     || startsWith(normalizedName, "die_"))
            {
                chooseBest(
                    manifest.die,
                    dieRank,
                    originalName,
                    variantRank(normalizedName, "die"));
            }
            else if (startsWith(normalizedName, "attack_begin"))
            {
                chooseBest(
                    manifest.attackBegin,
                    attackBeginRank,
                    originalName,
                    variantRank(normalizedName, "attack_begin"));
            }
            else if (startsWith(normalizedName, "attack_end"))
            {
                chooseBest(
                    manifest.attackEnd,
                    attackEndRank,
                    originalName,
                    variantRank(normalizedName, "attack_end"));
            }
            else if (normalizedName == "attack"
                     || startsWith(normalizedName, "attack_"))
            {
                // A 变体固定优先；没有 A 时再退回字典序最小的其他变体。
                chooseBest(
                    manifest.attack,
                    attackRank,
                    originalName,
                    variantRank(normalizedName, "attack"));
            }
            else if (normalizedName == "skill_1_start")
            {
                skillAttackBegin = originalName;
            }
            else if (normalizedName == "skill_1_loop")
            {
                skillAttackLoop = originalName;
            }
            else if (normalizedName == "skill_1_end")
            {
                skillAttackEnd = originalName;
            }
        }

        // 少数老师素材没有 Attack 命名，而以 Skill_1_Start/Loop/End
        // 表示同一套完整攻击动作。只在没有标准 Attack 时启用通用回退。
        if (manifest.attack.empty() && !skillAttackLoop.empty())
        {
            manifest.attack = skillAttackLoop;
            if (!skillAttackBegin.empty() && !skillAttackEnd.empty())
            {
                manifest.attackBegin = skillAttackBegin;
                manifest.attackEnd = skillAttackEnd;
            }
        }

        return manifest;
    }
}

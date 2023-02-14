#include "Interfaces.h"
#include "Memory.h"

#include "Logger.h"

#include "SDK/ConVar.h"
#include "SDK/GameEvent.h"
#include "SDK/Engine.h"
#include "SDK/Entity.h"
#include "SDK/LocalPlayer.h"

std::string getStringFromHitgroup(int hitgroup) noexcept
{
    switch (hitgroup) {
    case HitGroup::Generic:
        return "通用";
    case HitGroup::Head:
        return "头";
    case HitGroup::Chest:
        return "胸部";
    case HitGroup::Stomach:
        return "胃";
    case HitGroup::LeftArm:
        return "左臂";
    case HitGroup::RightArm:
        return "右臂";
    case HitGroup::LeftLeg:
        return "左腿";
    case HitGroup::RightLeg:
        return "右腿";
    default:
        return "未知的";
    }
}

void Logger::reset() noexcept
{
    renderLogs.clear();
    logs.clear();
}

void Logger::getEvent(GameEvent* event) noexcept
{
    if (!config->misc.logger.enabled || config->misc.loggerOptions.modes == 0 || config->misc.loggerOptions.events == 0)
    {
        logs.clear();
        renderLogs.clear();
        return;
    }

    if (!event || !localPlayer || interfaces->engine->isHLTV())
        return;

    static auto c4Timer = interfaces->cvar->findVar("c4计时器");

    Log log;
    log.time = memory->globalVars->realtime;

    switch (fnv::hashRuntime(event->getName())) {
    case fnv::hash("玩家伤害"):  {

        const int hurt = interfaces->engine->getPlayerForUserID(event->getInt("用户ID"));
        const int attack = interfaces->engine->getPlayerForUserID(event->getInt("攻击者"));
        const auto damage = std::to_string(event->getInt("生命值"));
        const auto hitgroup = getStringFromHitgroup(event->getInt("打击次数"));

        if (hurt != localPlayer->index() && attack == localPlayer->index())
        {
            if ((config->misc.loggerOptions.events & 1 << DamageDealt) != 1 << DamageDealt)
                break;

            const auto player = interfaces->entityList->getEntity(hurt);
            if (!player)
                break;

            log.text = "Hurt " + player->getPlayerName() + " for " + damage + " in " + hitgroup;
        }
        else if (hurt == localPlayer->index() && attack != localPlayer->index())
        {
            if ((config->misc.loggerOptions.events & 1 << DamageReceived) != 1 << DamageReceived)
                break;

            const auto player = interfaces->entityList->getEntity(attack);
            if (!player)
                break;

            log.text = "受到的伤害 " + player->getPlayerName() + " for " + damage + " in " + hitgroup;
        }
        break;
    }
    case fnv::hash("炸弹_植入"): {
        if ((config->misc.loggerOptions.events & 1 << BombPlants) != 1 << BombPlants)
            break;

        const int idx = interfaces->engine->getPlayerForUserID(event->getInt("userid"));
        if (idx == localPlayer->index())
            break;

        const auto player = interfaces->entityList->getEntity(idx);
        if (!player)
            break;

        const std::string site = event->getInt("地点") ? "a" : "b";

        log.text = "炸弹放置在爆炸现场 " + site + " by " + player->getPlayerName() + ", 引爆 " + std::to_string(c4Timer->getFloat()) + " 秒";
        break;
    }
    case fnv::hash("主机_允许"): {
        if ((config->misc.loggerOptions.events & 1 << HostageTaken) != 1 << HostageTaken)
            break;

        const int idx = interfaces->engine->getPlayerForUserID(event->getInt("用户ID"));
        if (idx == localPlayer->index())
            break;

        const auto player = interfaces->entityList->getEntity(idx);
        if (!player)
            break;

        log.text = "劫持人质者 " + player->getPlayerName();
        break;
    }
    default:
        return;
    }

    if (log.text.empty())
        return;

    logs.push_front(log);
    renderLogs.push_front(log);
}

void Logger::process(ImDrawList* drawList) noexcept
{
    if (!config->misc.logger.enabled || config->misc.loggerOptions.modes == 0 || config->misc.loggerOptions.events == 0)
    {
        logs.clear();
        renderLogs.clear();
        return;
    }

    console();
    render(drawList);
}

void Logger::console() noexcept
{
    if (logs.empty())
        return;

    if ((config->misc.loggerOptions.modes & 1 << Console) != 1 << Console)
    {
        logs.clear();
        return;
    }

    std::array<std::uint8_t, 4> color;
    if (!config->misc.logger.rainbow)
    {
        color.at(0) = static_cast<uint8_t>(config->misc.logger.color.at(0) * 255.0f);
        color.at(1) = static_cast<uint8_t>(config->misc.logger.color.at(1) * 255.0f);
        color.at(2) = static_cast<uint8_t>(config->misc.logger.color.at(2) * 255.0f);
    }
    else
    {
        const auto [colorR, colorG, colorB] { rainbowColor(config->misc.logger.rainbowSpeed) };
        color.at(0) = static_cast<uint8_t>(colorR * 255.0f);
        color.at(1) = static_cast<uint8_t>(colorG * 255.0f);
        color.at(2) = static_cast<uint8_t>(colorB * 255.0f);
    }
    color.at(3) = static_cast<uint8_t>(255.0f);

    for (auto log : logs)
        Helpers::logConsole(log.text + "\n", color);

    logs.clear();
}

void Logger::render(ImDrawList* drawList) noexcept
{
    if ((config->misc.loggerOptions.modes & 1 << EventLog) != 1 << EventLog)
    {
        renderLogs.clear();
        return;
    }

    if (renderLogs.empty())
        return;

    while (renderLogs.size() > 6)
        renderLogs.pop_back();

    for (int i = renderLogs.size() - 1; i >= 0; i--)
    {
        if (renderLogs[i].time + 5.0f <= memory->globalVars->realtime)
            renderLogs[i].alpha -= 16.f;

        const auto alphaBackup = Helpers::getAlphaFactor();
        Helpers::setAlphaFactor(renderLogs[i].alpha / 255.0f);
        const auto color = Helpers::calculateColor(config->misc.logger);
        Helpers::setAlphaFactor(alphaBackup);
        drawList->AddText(ImVec2{ 14.0f, 5.0f + static_cast<float>(20 * i) + 1.0f }, color & IM_COL32_A_MASK, renderLogs[i].text.c_str());
        drawList->AddText(ImVec2{ 14.0f, 5.0f + static_cast<float>(20 * i) + 1.0f }, color, renderLogs[i].text.c_str());
    }

    for (int i = renderLogs.size() - 1; i >= 0; i--) {
        if (renderLogs[i].alpha <= 0.0f) {
            renderLogs.erase(renderLogs.begin() + i);
            break;
        }
    }
}

void Logger::addLog(std::string logText) noexcept
{
    Log log;
    log.time = memory->globalVars->realtime;
    log.text = logText;

    logs.push_front(log);
    renderLogs.push_front(log);
}

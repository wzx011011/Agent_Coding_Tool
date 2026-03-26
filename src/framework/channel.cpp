#include "framework/channel.h"

namespace act::framework
{

IChannel::IChannel(const QString &channelId, QObject *parent)
    : QObject(parent)
    , m_channelId(channelId)
{
}

QString IChannel::channelId() const
{
    return m_channelId;
}

} // namespace act::framework

--- 日志服务
-- @module logger
-- @author <zhenghongwei@onemt.com.cn>
require 'skynet.manager'
local skynet = require 'skynet'

-- Log请在preload中require
Log.init(3, {
    {
        dir = "./data/log",
        fname = "skynet",
        fsize = 100,
    },
    {
        level = Log.INFO,
        dir = "./data/bigdata/sync_log",
        fname = "sync",
        cuthour = 1,
    },
    {
        level = Log.INFO,
        dir = "./data/bigdata/async_log",
        fname = "async",
        cuthour = 1,
    },
})

skynet.register_protocol({
    name = 'text',
    id = skynet.PTYPE_TEXT,
    unpack = skynet.tostring,
    dispatch = function(_, source, msg)
        Log.i(source, msg)
    end,
})

skynet.start(function()
    skynet.register('.logger')
end)


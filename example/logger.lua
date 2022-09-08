--- 日志服务
-- 简单的skynet日志服务的例子
-- @module logger
-- @author <zhenghongwei@onemt.com.cn>
require 'skynet.manager'
local skynet = require 'skynet'

-- Log请在preload中require
Log.init(3, {{
    dir = "./data/log",
    fname = "test",
    fsize = 100,
}, {
    level = Log.INFO,
    dir = "./data/sync_log",
    fname = "nid1",
    cuthour = 1,
}, {
    level = Log.INFO,
    dir = "./data/async_log",
    fname = "kid1",
    cuthour = 1,
}})

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


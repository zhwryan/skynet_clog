--- 压测clog
-- @module pressclog
-- @author <zhenghongwei@onemt.com.cn>
-- @usage log pressclog 8000

local skynet = require "skynet"

local count, logcd = ...
count = tonumber(count or 1)
logcd = tonumber(logcd or 10)

skynet.start(function()
	for i=1, count do
        skynet.newservice("testclog", "press", logcd, i)
    end
end)



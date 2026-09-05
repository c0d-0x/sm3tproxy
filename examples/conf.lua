local send_json = function()
	return [[
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 120

{"current_weather":{"temperature":15.3,"windspeed":12.5,"winddirection":220,"weathercode":3,"time":"2026-01-09T12:00"}}
]]
end

return {
	sys = {
		name = "c0d_0x.domain",
		user = "c0d_0x",
		group = "sm3tproxy",
		chroot = "/",
		logging = {
			level = 0,
			format = 0,
			file_path = "",
			out = {
				file = false,
				stdout = true,
				syslog = false,
			},
		},
	},

	tcp = {
		orig_dst = 1,
		backpressure = 0,
		listen = {
			mode = 2,
			port = 6969,
			address = "127.0.0.1",
		},

		forward = {
			upstreams = {
				{ name = "localhost", ver = 4, address = "127.0.0.1", port = 6600 },
			},
			upstreams_count = 1,
			failure_policy = 0,
		},

		conn = {
			limit = {
				max_total = 0,
				max_per_src = 0,
			},

			timeout = {
				connect_ms = 0,
				idle_ms = 0,
				lifetime_ms = 0,
			},

			retries = {
				enable = false,
				max = 0,
				backoff_ms = 0,
			},
		},

		transport = {
			keepalive = {
				enable = false,
				idle_ms = 0,
				interval_ms = 0,
				count = 0,
			},

			nodelay = false,
			reset_on_violation = false,
		},

		telemetry = {
			enable = false,
			metrics = {
				per_src = false,
				per_dst = false,
			},

			log = {
				src_ip = false,
				src_port = false,
				dst_ip = false,
				dst_port = false,
				bytes_in = false,
				bytes_out = false,
				duration = false,
				err = false,
			},
		},
	},

	-- TODO: Will move hooks to proxy.on_**()
	hooks = {
		on_connect = function(ctx)
			local addr = proxy.rhost(ctx)
			local port = proxy.rport(ctx)

			local msg = "New Context: " .. addr .. ":" .. port
			proxy.log(msg)
			return true
		end,

		on_data = function(ctx, data, direction)
			local addr = proxy.rhost(ctx)
			local port = proxy.rport(ctx)

			local msg = "data recieved from: " .. addr .. ":" .. port
			proxy.log(msg)
			return data
		end,

		on_disconnect = function(ctx)
			local addr = proxy.rhost(ctx)
			local port = proxy.rport(ctx)
			local msg = "context dropped: " .. addr .. ":" .. port

			proxy.log(msg)
			return true
		end,
	},
}

local conf = {
	sys = {
		name = "",
		user = "",
		group = "sm3tproxy",
		chroot = "",
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
		listen = {
			port = 8080,
			address = "127.0.0.1",
		},

		orig_dst = {
			required = false,
			optional = false,
			ignored = false,
		},

		allow = {
			src = {
				cidrs = {},
				ports = {},
				cidrs_count = 0,
				ports_count = 0,
			},

			dst = {
				cidrs = {},
				ports = {},
				cidrs_count = 0,
				ports_count = 0,
			},
		},

		deny = {
			src = {
				cidrs = {},
				ports = {},
				cidrs_count = 0,
				ports_count = 0,
			},

			dst = {
				cidrs = {},
				cidrs_count = 0,
				ports = {},
				ports_count = 0,
			},
		},

		forward = {
			mode = 0,
			upstreams = {},
			upstreams_count = 0,
			failure_policy = {
				drop = false,
				reset = false,
				bypass = false,
			},
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

		flow_control = {
			backpressure_stall = false,
			backpressure_close = false,
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
}

return conf

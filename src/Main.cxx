/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "Main.hxx"
#include "Instance.hxx"
#include "CommandLine.hxx"
#include "PlaylistFile.hxx"
#include "MusicChunk.hxx"
#include "StateFile.hxx"
#include "Mapper.hxx"
#include "Permission.hxx"
#include "Listen.hxx"
#include "client/Config.hxx"
#include "client/List.hxx"
#include "command/AllCommands.hxx"
#include "Partition.hxx"
#include "tag/Config.hxx"
#include "ReplayGainGlobal.hxx"
#include "IdleFlags.hxx"
#include "Log.hxx"
#include "LogInit.hxx"
#include "input/Init.hxx"
#include "input/cache/Config.hxx"
#include "input/cache/Manager.hxx"
#include "event/Loop.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/Config.hxx"
#include "playlist/PlaylistRegistry.hxx"
#include "zeroconf/ZeroconfGlue.hxx"
#include "decoder/DecoderList.hxx"
#include "pcm/AudioParser.hxx"
#include "pcm/Convert.hxx"
#include "unix/SignalHandlers.hxx"
#include "thread/Slack.hxx"
#include "net/Init.hxx"
#include "lib/icu/Init.hxx"
#include "config/Check.hxx"
#include "config/Data.hxx"
#include "config/Param.hxx"
#include "config/Path.hxx"
#include "config/Defaults.hxx"
#include "config/Option.hxx"
#include "config/Domain.hxx"
#include "config/Parser.hxx"
#include "util/RuntimeError.hxx"
#include "util/ScopeExit.hxx"
#include "util/ConvertStr.hxx"
#include <iostream>

#ifdef ENABLE_DAEMON
#include "unix/Daemon.hxx"
#endif

#ifdef ENABLE_DATABASE
#include "db/update/Service.hxx"
#include "db/Configured.hxx"
#include "db/DatabasePlugin.hxx"
#include "db/plugins/simple/SimpleDatabasePlugin.hxx"
#include "storage/Configured.hxx"
#include "storage/CompositeStorage.hxx"
#ifdef ENABLE_INOTIFY
#include "db/update/InotifyUpdate.hxx"
#endif
#endif

#ifdef ENABLE_NEIGHBOR_PLUGINS
#include "neighbor/Glue.hxx"
#endif

#ifdef ENABLE_SQLITE
#include "sticker/Database.hxx"
#endif

#ifdef ENABLE_ARCHIVE
#include "archive/ArchiveList.hxx"
#endif

#ifdef ANDROID
#include "java/Global.hxx"
#include "java/File.hxx"
#include "android/Environment.hxx"
#include "android/Context.hxx"
#include "android/LogListener.hxx"
#include "config/File.hxx"
#include "fs/FileSystem.hxx"
#include "org_musicpd_Bridge.h"
#endif

#ifdef ENABLE_DBUS
#include "lib/dbus/Init.hxx"
#endif

#ifdef ENABLE_SYSTEMD_DAEMON
#include <systemd/sd-daemon.h>
#endif

#include <climits>

#ifndef ANDROID
#include <clocale>
#endif

#include <jansson.h>
#include "command/OtherCommands.hxx"
#include "MaMpdMute.hxx"
void main_loop0(void);

static constexpr size_t KILOBYTE = 1024;
static constexpr size_t MEGABYTE = 1024 * KILOBYTE;

static constexpr size_t DEFAULT_BUFFER_SIZE = 4 * MEGABYTE;

static constexpr
size_t MIN_BUFFER_SIZE = std::max(CHUNK_SIZE * 32,
				  64 * KILOBYTE);

#ifdef ANDROID
Context *context;
LogListener *logListener;
#endif

Instance *global_instance;

struct Config {
	ReplayGainConfig replay_gain;

	explicit Config(const ConfigData &raw)
		:replay_gain(LoadReplayGainConfig(raw)) {}
};

#ifdef ENABLE_DAEMON

static void
glue_daemonize_init(const struct options *options,
		    const ConfigData &config)
{
	daemonize_init(config.GetString(ConfigOption::USER),
		       config.GetString(ConfigOption::GROUP),
		       config.GetPath(ConfigOption::PID_FILE));

	if (options->kill)
		daemonize_kill();
}

#endif
static int mpd_init_device(void)
{
	const char *fileCfg = "/mnt/mmcblk4/mads/mads.cfg";
	json_t *item;
	json_error_t error;
	item = json_load_file(fileCfg, 0, &error);
	if (!item) 
	{
		FormatNotice(config_domain, "mpd load /mnt/mmcblk4/mads/mads.cfg failed");
		//return -1;
	}
	else
	{
		const char *pValue = json_string_value(json_object_get(item, "hostname"));
		if(pValue)FormatNotice(config_domain, "hostname %s", pValue);
		if(pValue && ( strncmp(pValue, "ELEMENT-S", 9) == 0 ||
			strncmp(pValue, "NT-1", 4) == 0 ||
			strncmp(pValue, "TT-1", 4) == 0 ) )
		{
			// const char *pval = json_string_value(json_object_get(item, "softwareVolumeStatus"));
			// if(pval != NULL && 0 == strcmp(pval, "on"))
			// {
			// 	FormatNotice(config_domain, "softwareVolumeStatus on");
			// 	//mpd_softwareVolumeStatus_set(true);
			// }
			// else
			// {
			// 	//mpd_softwareVolumeStatus_set(false);
			// 	FormatNotice(config_domain, "softwareVolumeStatus off");
			// }

			json_t *softwareVolume = json_object_get(item, "softwareVolume");
			if (softwareVolume)
			{
				int vol_t = (json_integer_value(softwareVolume));
				mpd_softwareVolume_set(vol_t);
				FormatNotice(config_domain, "softwareVolume %d", vol_t);
			}

			// json_t *maDacDelayStatus = json_object_get(item, "maDacDelayStatus");
			// if (maDacDelayStatus)
			// {
			// 	std::string sbuf = (json_string_value(maDacDelayStatus));
			// 	handle_mpd_mute_setDacDelay(sbuf);
			// 	FormatNotice(config_domain, "maDacDelayStatus %s", sbuf.c_str());
			// }

		    // json_t *output = json_object_get(item, "output");
			// if (output)
			// {
			// 	std::string sbuf = (json_string_value(output));
			// 	handle_mpd_mute_setOutput(sbuf);
			// 	FormatNotice(config_domain, "output %s", sbuf.c_str());
			// }
		}
		json_decref(item);
	}



	//读取配置文件
	const char *maMpdFileCfg = "/mnt/mmcblk4/matrix_mpd.cfg";
	json_t *item_1_;
	json_error_t error_1_;
	item_1_ = json_load_file(maMpdFileCfg, 0, &error_1_);
	if (!item_1_) 
	{
		FormatNotice(config_domain, "mpd load /mnt/mmcblk4/matrix_mpd.cfg failed");
		//return -1;
	}
	else
	{
		json_t *maDacDelayStatus = json_object_get(item_1_, "maDacDelayStatus");
		if (maDacDelayStatus)
		{
			std::string sbuf = (json_string_value(maDacDelayStatus));
			handle_mpd_mute_setDacDelay(sbuf);
			FormatNotice(config_domain, "maDacDelayStatus %s", sbuf.c_str());
		}

		json_t *output = json_object_get(item_1_, "output");
		if (output)
		{
			std::string sbuf = (json_string_value(output));
			handle_mpd_mute_setOutput(sbuf);
			FormatNotice(config_domain, "output %s", sbuf.c_str());
		}

		json_t *usblimit = json_object_get(item_1_, "USBDACOutputPCMRateLimit");
		if (usblimit)
		{
			std::string sbuf = (json_string_value(usblimit));
			handle_mpd_mute_setUsbLimit(sbuf);
			FormatNotice(config_domain, "USBDACOutputPCMRateLimit %s", sbuf.c_str());
		}

		json_decref(item_1_);
	}

	return 0;
}

static void
glue_mapper_init(const ConfigData &config)
{
	mapper_init(config.GetPath(ConfigOption::PLAYLIST_DIR));
}

#ifdef ENABLE_DATABASE

static void
InitStorage(Instance &instance, EventLoop &event_loop,
	    const ConfigData &config)
{
	auto storage = CreateConfiguredStorage(config, event_loop);
	if (storage == nullptr)
		return;

	auto *composite = new CompositeStorage();
	instance.storage = composite;
	composite->Mount("", std::move(storage));
}

/**
 * Returns the database.  If this function returns false, this has not
 * succeeded, and the caller should create the database after the
 * process has been daemonized.
 */
static bool
glue_db_init_and_load(Instance &instance, const ConfigData &config)
{
	auto db = CreateConfiguredDatabase(config, instance.event_loop,
					   instance.io_thread.GetEventLoop(),
					   instance);
	if (!db)
		return true;

	if (db->GetPlugin().RequireStorage()) {
		InitStorage(instance, instance.io_thread.GetEventLoop(),
			    config);

		if (instance.storage == nullptr) {
			LogNotice(config_domain,
				  "Found database setting without "
				  "music_directory - disabling database");
			return true;
		}
	} else {
		if (IsStorageConfigured(config))
			LogNotice(config_domain,
				  "Ignoring the storage configuration "
				  "because the database does not need it");
	}

	try {
		db->Open();
	} catch (...) {
		std::throw_with_nested(std::runtime_error("Failed to open database plugin"));
	}

	instance.database = std::move(db);

	auto *sdb = dynamic_cast<SimpleDatabase *>(instance.database.get());
	if (sdb == nullptr)
		return true;

	instance.update = new UpdateService(config,
					    instance.event_loop, *sdb,
					    static_cast<CompositeStorage &>(*instance.storage),
					    instance);

	/* run database update after daemonization? */
	return sdb->FileExists();
}

static bool
InitDatabaseAndStorage(Instance &instance, const ConfigData &config)
{
	const bool create_db = !glue_db_init_and_load(instance, config);
	return create_db;
}

#endif

#ifdef ENABLE_SQLITE

/**
 * Configure and initialize the sticker subsystem.
 */
static std::unique_ptr<StickerDatabase>
LoadStickerDatabase(const ConfigData &config)
{
	auto sticker_file = config.GetPath(ConfigOption::STICKER_FILE);
	if (sticker_file.IsNull())
		return nullptr;

	return std::make_unique<StickerDatabase>(std::move(sticker_file));
}

#endif

static void
glue_state_file_init(Instance &instance, const ConfigData &raw_config)
{
	StateFileConfig config(raw_config);
	if (!config.IsEnabled())
		return;

	instance.state_file = std::make_unique< StateFile>(std::move(config),
							   instance.partitions.front(),
							   instance.event_loop);
	instance.state_file->Read();
}

/**
 * Initialize the decoder and player core, including the music pipe.
 */
static void
initialize_decoder_and_player(Instance &instance,
			      const ConfigData &config,
			      const ReplayGainConfig &replay_gain_config)
{
	const ConfigParam *param;

	size_t buffer_size;
	param = config.GetParam(ConfigOption::AUDIO_BUFFER_SIZE);
	if (param != nullptr) {
		buffer_size = param->With([](const char *s){
			size_t result = ParseSize(s, KILOBYTE);
			if (result <= 0)
				throw FormatRuntimeError("buffer size \"%s\" is not a "
							 "positive integer", s);

			if (result < MIN_BUFFER_SIZE) {
				FormatWarning(config_domain, "buffer size %lu is too small, using %lu bytes instead",
					      (unsigned long)result,
					      (unsigned long)MIN_BUFFER_SIZE);
				result = MIN_BUFFER_SIZE;
			}

			return result;
		});
	} else
		buffer_size = DEFAULT_BUFFER_SIZE;

	const unsigned buffered_chunks = buffer_size / CHUNK_SIZE;

	if (buffered_chunks >= 1 << 15)
		throw FormatRuntimeError("buffer size \"%lu\" is too big",
					 (unsigned long)buffer_size);

	const unsigned max_length =
		config.GetPositive(ConfigOption::MAX_PLAYLIST_LENGTH,
				   DEFAULT_PLAYLIST_MAX_LENGTH);

	AudioFormat configured_audio_format = config.With(ConfigOption::AUDIO_OUTPUT_FORMAT, [](const char *s){
		if (s == nullptr)
			return AudioFormat::Undefined();

		return ParseAudioFormat(s, true);
	});

	instance.partitions.emplace_back(instance,
					 "default",
					 max_length,
					 buffered_chunks,
					 configured_audio_format,
					 replay_gain_config);
	auto &partition = instance.partitions.back();

	partition.replay_gain_mode = config.With(ConfigOption::REPLAYGAIN, [](const char *s){
		return s != nullptr
			? FromString(s)
			: ReplayGainMode::OFF;
	});
}

inline void
Instance::BeginShutdownUpdate() noexcept
{
#ifdef ENABLE_DATABASE
#ifdef ENABLE_INOTIFY
	mpd_inotify_finish();
#endif

	if (update != nullptr)
		update->CancelAllAsync();
#endif
}

inline void
Instance::BeginShutdownPartitions() noexcept
{
	for (auto &partition : partitions) {
		partition.BeginShutdown();
	}
}

static inline void
MainConfigured(const struct options &options, const ConfigData &raw_config)
{
	mpd_init_device();
#ifdef ENABLE_DAEMON
	daemonize_close_stdin();
#endif

#ifndef ANDROID
	/* initialize locale */
	std::setlocale(LC_CTYPE,"");
	std::setlocale(LC_COLLATE, "");
#endif

	const ScopeIcuInit icu_init;
	const ScopeNetInit net_init;

#ifdef ENABLE_DBUS
	const ODBus::ScopeInit dbus_init;
#endif

	InitPathParser(raw_config);
	const Config config(raw_config);

#ifdef ENABLE_DAEMON
	glue_daemonize_init(&options, raw_config);
#endif

	TagLoadConfig(raw_config);

	log_init(raw_config, options.verbose, options.log_stderr);

	Instance instance;
	global_instance = &instance;

#ifdef ENABLE_NEIGHBOR_PLUGINS
	instance.neighbors = std::make_unique<NeighborGlue>();
	instance.neighbors->Init(raw_config,
				 instance.io_thread.GetEventLoop(),
				 instance);

	if (instance.neighbors->IsEmpty())
		instance.neighbors.reset();
#endif

	const unsigned max_clients =
		raw_config.GetPositive(ConfigOption::MAX_CONN, 100);
	instance.client_list = std::make_unique<ClientList>(max_clients);

	const auto *input_cache_config = raw_config.GetBlock(ConfigBlockOption::INPUT_CACHE);
	if (input_cache_config != nullptr) {
		const InputCacheConfig c(*input_cache_config);
		instance.input_cache = std::make_unique<InputCacheManager>(c);
	}

	initialize_decoder_and_player(instance,
				      raw_config, config.replay_gain);

	listen_global_init(raw_config, *instance.partitions.front().listener);

#ifdef ENABLE_DAEMON
	daemonize_set_user();
	daemonize_begin(options.daemon);
	AtScopeExit() { daemonize_finish(); };
#endif

	ConfigureFS(raw_config);
	AtScopeExit() { DeinitFS(); };

	glue_mapper_init(raw_config);

	initPermissions(raw_config);
	spl_global_init(raw_config);
#ifdef ENABLE_ARCHIVE
	const ScopeArchivePluginsInit archive_plugins_init;
#endif

	pcm_convert_global_init(raw_config);

	const ScopeDecoderPluginsInit decoder_plugins_init(raw_config);

#ifdef ENABLE_DATABASE
	const bool create_db = InitDatabaseAndStorage(instance, raw_config);
#endif

#ifdef ENABLE_SQLITE
	instance.sticker_database = LoadStickerDatabase(raw_config);
#endif

	command_init();

	for (auto &partition : instance.partitions) {
		partition.outputs.Configure(instance.rtio_thread.GetEventLoop(),
					    raw_config,
					    config.replay_gain);
		partition.UpdateEffectiveReplayGainMode();
	}

	client_manager_init(raw_config);
	const ScopeInputPluginsInit input_plugins_init(raw_config,
						       instance.io_thread.GetEventLoop());

	const ScopePlaylistPluginsInit playlist_plugins_init(raw_config);

#ifdef ENABLE_DAEMON
	daemonize_commit();
#endif
	main_loop0();
#ifndef ANDROID
	setup_log_output();

	const ScopeSignalHandlersInit signal_handlers_init(instance);
#endif

	instance.io_thread.Start();
	instance.rtio_thread.Start();

#ifdef ENABLE_NEIGHBOR_PLUGINS
	if (instance.neighbors != nullptr)
		instance.neighbors->Open();

	AtScopeExit(&instance) {
		if (instance.neighbors != nullptr)
			instance.neighbors->Close();
	};
#endif

	ZeroconfInit(raw_config, instance.event_loop);

#ifdef ENABLE_DATABASE
	if (create_db) {
		/* the database failed to load: recreate the
		   database */
		instance.update->Enqueue("", true);
	}
#endif

	glue_state_file_init(instance, raw_config);

#ifdef ENABLE_DATABASE
	if (raw_config.GetBool(ConfigOption::AUTO_UPDATE, false)) {
#ifdef ENABLE_INOTIFY
		if (instance.storage != nullptr &&
		    instance.update != nullptr)
			mpd_inotify_init(instance.event_loop,
					 *instance.storage,
					 *instance.update,
					 raw_config.GetUnsigned(ConfigOption::AUTO_UPDATE_DEPTH,
								INT_MAX));
#else
		FormatWarning(config_domain,
			      "inotify: auto_update was disabled. enable during compilation phase");
#endif
	}
#endif

	Check(raw_config);

	/* enable all audio outputs (if not already done by
	   playlist_state_restore() */
	for (auto &partition : instance.partitions)
		partition.pc.LockUpdateAudio();

#ifdef _WIN32
	win32_app_started();
#endif

	/* the MPD frontend does not care about timer slack; set it to
	   a huge value to allow the kernel to reduce CPU wakeups */
	SetThreadTimerSlack(std::chrono::milliseconds(100));

#ifdef ENABLE_SYSTEMD_DAEMON
	sd_notify(0, "READY=1");
#endif

	/* run the main loop */
	instance.event_loop.Run();

#ifdef _WIN32
	win32_app_stopping();
#endif

	/* cleanup */

	if (instance.state_file)
		instance.state_file->Write();

	instance.BeginShutdownUpdate();

	ZeroconfDeinit();

	instance.BeginShutdownPartitions();
}

#ifdef ANDROID

static void
AndroidMain()
{
	struct options options;
	ConfigData raw_config;

	const auto sdcard = Environment::getExternalStorageDirectory();
	if (!sdcard.IsNull()) {
		const auto config_path =
			sdcard / Path::FromFS("mpd.conf");
		if (FileExists(config_path))
			ReadConfigFile(raw_config, config_path);
	}

	MainConfigured(options, raw_config);
}

gcc_visibility_default
JNIEXPORT void JNICALL
Java_org_musicpd_Bridge_run(JNIEnv *env, jclass, jobject _context, jobject _logListener)
{
	Java::Init(env);
	Java::Object::Initialise(env);
	Java::File::Initialise(env);
	Environment::Initialise(env);
	AtScopeExit(env) { Environment::Deinitialise(env); };

	context = new Context(env, _context);
	AtScopeExit() { delete context; };

	if (_logListener != nullptr)
		logListener = new LogListener(env, _logListener);
	AtScopeExit() { delete logListener; };

	try {
		AndroidMain();
	} catch (...) {
		LogError(std::current_exception());
	}
}

gcc_visibility_default
JNIEXPORT void JNICALL
Java_org_musicpd_Bridge_shutdown(JNIEnv *, jclass)
{
	if (global_instance != nullptr)
		global_instance->Break();
}

#else

static inline void
MainOrThrow(int argc, char *argv[])
{
	struct options options;
	ConfigData raw_config;

	ParseCommandLine(argc, argv, options, raw_config);

	MainConfigured(options, raw_config);
}

#include <sstream>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <thread>

#define MXA_CON_NUM 5
int init_usock()
{
	int fd;
	struct sockaddr_un server_un;
	const char *sock_filename = "/opt/mpdStream-sock";
	fprintf(stderr, "create usock\n");
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if(fd < 0){
		fprintf(stderr,  "mads usock failed\n");
		return -1;
	}
	fprintf(stderr, "bind usock0\n");
	server_un.sun_family = AF_UNIX;
	unlink(sock_filename);
	
	strcpy(server_un.sun_path, sock_filename);
	fprintf(stderr, "bind usock\n");
	if(bind(fd, (struct sockaddr *)&server_un, sizeof(server_un)) < 0){
		fprintf(stderr, "mads usock bind failed\n");
		close(fd);
		return -1;
	}
	
	if(listen(fd, MXA_CON_NUM) < 0){
		fprintf(stderr, "mads usock listen failed\n");
		close(fd);
		return -1;
	}
	fprintf(stderr, "ok usock\n");
	return fd;
}

void * main_loop_usock(void *arg)
{
	(void)arg;
	int ufd = init_usock();
	if(ufd <= 0){
		fprintf(stderr, "listen usock err:%d\n", ufd);
		return nullptr;
	}
	//add_epool_fd(epfd, ufd);
	const int BUFFER_SIZE = 4096;
	while(1){
        struct sockaddr_un client_addr;
        char buffer[BUFFER_SIZE];
        bzero(buffer,BUFFER_SIZE);
        socklen_t len = sizeof(client_addr);
        int new_fd = accept(ufd,(struct sockaddr *)&client_addr,&len);
        //new_fd = accept(fd,NULL,NULL);
        if(new_fd < 0){
            fprintf(stderr, "accept failed\n");
        }
        int res = read(new_fd,buffer,BUFFER_SIZE);
		
		if(buffer[ res - 1] == '\n' || buffer[ res - 1] == '\r')
		{
			buffer[res - 1] = '\0';
			
			std::string str1 = buffer;
			if(str1.compare(0, 8, "qqpasswd") == 0)
			{
				std::stringstream ss(str1);
				std::string passwd, vect, restr, qqTrackType;
				int i = 0;
				while(getline(ss, restr, ' '))
				{
					if(i == 1) passwd = restr;
					if(i == 2) vect = restr;
					if(i == 3) qqTrackType = restr;
					i++;
				}
				//fprintf(stderr, "pass %s %s\n", passwd.c_str(), vect.c_str());
				if(passwd.size() > 0 && vect.size() > 0)
					handle_set_streamKey(passwd, vect);
				if(qqTrackType.size() > 0)
					handle_set_qqTrackType(qqTrackType);
			}
			else
			{
				json_error_t error;
				json_t *json;
				json = json_loadb(buffer,  res - 1, 0, &error);
				if (!json) {
					fprintf(stderr, "json_loadb failed, (%s):%d: %s", buffer, error.line, error.text);
				}
				fprintf(stderr, "cmd:%s", buffer);
				const char *_cmd = json_string_value(json_object_get(json, "cmd"));
				if(_cmd != nullptr && strcmp(_cmd, "setAlsa") == 0)
				{
					const char *sndName = json_string_value(json_object_get(json, "sndName"));
					const char *DSDType = json_string_value(json_object_get(json, "DSDType"));
					const char *softwareVol = json_string_value(json_object_get(json, "softwareVol"));
					const char *allowFormat = json_string_value(json_object_get(json, "allowFormat"));
					if(sndName != nullptr && strlen(sndName) > 0)
					{
						std::string sn(sndName);
						handle_set_sndName(sn);
					}
					if(DSDType != nullptr && strlen(DSDType) > 0)
					{
						std::string dt(DSDType);
						handle_set_DSDType(dt);
					}
					if(softwareVol != nullptr && strlen(softwareVol) > 0)
					{
						std::string sv(softwareVol);
						handle_set_softwareVol(sv);
					}
					if(allowFormat != nullptr && strlen(allowFormat) > 0)
					{
						std::string af(allowFormat);
						handle_set_allowFormat(af);
					}
				}

				json_decref(json);
			}

		}
		write(new_fd, "OK\r\nOK\r\n", 8);
		close(new_fd);
	}
	return nullptr;
}

void main_loop0(void)
{
	//std::thread t1(main_loop_usock, "nullptr");
    //t1.detach();
	int res;
	pthread_t ctl_thd;
	pthread_attr_t ctl_attr;
	res = pthread_attr_init(&ctl_attr);
	if(res != 0){
		fprintf(stderr, "thread attribute creation failed\n");
		return ;
		//exit(EXIT_FAILURE);
	}
	res = pthread_attr_setdetachstate(&ctl_attr, PTHREAD_CREATE_DETACHED);
	if(res != 0){
		fprintf(stderr, "setting detache attribute failed\n");
		return ;
		//exit(EXIT_FAILURE);
	}
	res = pthread_create(&ctl_thd, &ctl_attr, main_loop_usock, nullptr);
	if(res != 0){
		fprintf(stderr, "create thread failed\n");
		return ;
		//exit(EXIT_FAILURE);
	}
}
int mpd_main(int argc, char *argv[]) noexcept
{
	AtScopeExit() { log_deinit(); };
	
	try {
		MainOrThrow(argc, argv);
		return EXIT_SUCCESS;
	} catch (...) {
		LogError(std::current_exception());
		return EXIT_FAILURE;
	}
}

int
main(int argc, char *argv[]) noexcept
{
#ifdef _WIN32
	return win32_main(argc, argv);
#else
	return mpd_main(argc, argv);
#endif
}

#endif

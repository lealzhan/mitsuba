/*
	Add by Lifan Wu
	Nov 22, 2015
*/

#include <mitsuba/core/plugin.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/render/volume.h>
#include <mitsuba/render/util.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/properties.h>

MTS_NAMESPACE_BEGIN

class ClampVolume : public Utility {
public:
	typedef std::vector<std::vector<std::vector<Vector> > > GridData;

	int run(int argc, char **argv) {
		if (argc != 5 && argc != 7) {
			cout << "Down-sample grid volume data by a scale" << endl;
			cout << "Syntax: mtsutil downSampleVolume 0 <grid_volume> <threshold> <target_volume>" << endl;
			cout << "Syntax: mtsutil downSampleVolume 1 <hgrid_volume_dict> <threshold> <prefix> <origin_suffix> <target_suffix>" << endl;
			return -1;
		}

		if (strcmp(argv[1], "0") == 0) {
			char *end_ptr = NULL;
			m_threshold = strtod(argv[3], &end_ptr);
			if (*end_ptr != '\0')
				SLog(EError, "Could not parse floating point value");

			Properties props("gridvolume");
			props.setString("filename", argv[2]);
			props.setBoolean("sendData", false);

			VolumeDataSource *originVol = static_cast<VolumeDataSource *> (PluginManager::getInstance()->
				createObject(MTS_CLASS(VolumeDataSource), props));
			originVol->configure();

			Log(EInfo, "%s", originVol->getClass()->getName().c_str());
			Log(EInfo, "res = (%d, %d, %d)", originVol->getResolution().x, originVol->getResolution().y, originVol->getResolution().z);
			Log(EInfo, "channels = %d", originVol->getChannels());

			AABB bbox = originVol->getAABB();

			GridData s;
			clampVolume(originVol, s);

			Log(EInfo, "finish down-sampling, save volume data to file");
			ref<FileStream> outFile = new FileStream(argv[4], FileStream::ETruncReadWrite);
			writeVolume(s, bbox, originVol->getChannels(), outFile);
		}
		else if (strcmp(argv[1], "1") == 0) {
			char *end_ptr = NULL;
			m_threshold = strtod(argv[3], &end_ptr);
			if (*end_ptr != '\0')
				SLog(EError, "Could not parse floating point value");

			fs::path resolved = Thread::getThread()->getFileResolver()->resolve(argv[2]);
			Log(EInfo, "Loading hierarchical grid dictrionary \"%s\"", argv[2]);
			ref<FileStream> stream = new FileStream(resolved, FileStream::EReadOnly);
			stream->setByteOrder(Stream::ELittleEndian);

			Float xmin = stream->readSingle(), ymin = stream->readSingle(), zmin = stream->readSingle();
			Float xmax = stream->readSingle(), ymax = stream->readSingle(), zmax = stream->readSingle();
			AABB aabb = AABB(Point(xmin, ymin, zmin), Point(xmax, ymax, zmax));

			Vector3i res = Vector3i(stream);
			int nCells = res.x * res.y * res.z;

			int numBlocks = 0;
			while (!stream->isEOF()) {
				Vector3i block = Vector3i(stream);
				Assert(block.x >= 0 && block.y >= 0 && block.z >= 0
					&& block.x < res.x && block.y < res.y && block.z < res.z);

				Properties props("gridvolume");
				props.setString("filename", formatString("%s%03i_%03i_%03i%s",
					argv[4], block.x, block.y, block.z, argv[5]));
				props.setBoolean("sendData", false);

				VolumeDataSource *ori = static_cast<VolumeDataSource *> (PluginManager::getInstance()->
					createObject(MTS_CLASS(VolumeDataSource), props));
				ori->configure();

				//Log(EInfo, "Loading grid %03i_%03i_%03i", block.x, block.y, block.z);

				AABB bbox = ori->getAABB();

				GridData s;
				clampVolume(ori, s);

				std::string filename(formatString("%s%03i_%03i_%03i%s", argv[4], block.x, block.y, block.z, argv[6]));
				ref<FileStream> outFile = new FileStream(filename.c_str(), FileStream::ETruncReadWrite);

				writeVolume(s, bbox, ori->getChannels(), outFile);

				++numBlocks;
			}
			Log(EInfo, "%i blocks total, %s, resolution=%s", numBlocks,
				aabb.toString().c_str(), res.toString().c_str());
		}

		return 0;
	}

	void initS(GridData &s, const Vector3i &res) {
		s.resize(res.x);
		for (int i = 0; i < res.x; i++) {
			s[i].resize(res.y);
			for (int j = 0; j < res.y; j++) {
				s[i][j].resize(res.z);
			}
		}
	}

	void clampVolume(VolumeDataSource *ori, GridData &s) {
		int channels = ori->getChannels();
		Vector3i res = ori->getResolution();

		initS(s, res);

		for (int i = 0; i < res.x; i++) {
			for (int j = 0; j < res.y; j++) {
				for (int k = 0; k < res.z; k++) {
					if (channels == 1) {
						// Only handle 1 channel
						float v = ori->lookupFloat(i, j, k, 0);
						if (v < m_threshold)
							v = 0;
						s[i][j][k] = Vector(v);
					}
					else {
						// Undefined behavior
						Vector v(ori->lookupFloat(i, j, k, 0),
						ori->lookupFloat(i, j, k, 1),
						ori->lookupFloat(i, j, k, 2));
					}
				}
			}
		}
	}

	void writeVolume(GridData &s, AABB &bbox, int channels, Stream *stream) {
		stream->writeChar('V');
		stream->writeChar('O');
		stream->writeChar('L');
		stream->writeChar(3);

		int type = 1;
		stream->writeInt(type);

		stream->writeInt(s.size());
		stream->writeInt(s[0].size());
		stream->writeInt(s[0][0].size());

		stream->writeInt(channels);

		stream->writeSingle(bbox.min.x);
		stream->writeSingle(bbox.min.y);
		stream->writeSingle(bbox.min.z);
		stream->writeSingle(bbox.max.x);
		stream->writeSingle(bbox.max.y);
		stream->writeSingle(bbox.max.z);

		float *data = new float[s.size() * s[0].size() * s[0][0].size() * channels];
		for (int z = 0; z < s[0][0].size(); z++) {
			for (int y = 0; y < s[0].size(); y++) {
				for (int x = 0; x < s.size(); x++) {
					for (int c = 0; c < channels; c++) {
						data[((z * s[0].size() + y) * s.size() + x) * channels + c] = s[x][y][z][c];
					}
				}
			}
		}
		stream->writeSingleArray(data, s.size() * s[0].size() * s[0][0].size() * channels);
		delete[] data;
	}

	Float m_threshold;

	MTS_DECLARE_UTILITY()
};

MTS_EXPORT_UTILITY(ClampVolume, "Clamp volume")
MTS_NAMESPACE_END
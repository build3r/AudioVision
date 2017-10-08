#include "pch.h"
#include "AudioCloudSource.h"

const int BUFFER_SIZE = 50;
const int SAMPLE_SIZE = 100;
const int SAMPLES_PER_CLUSTER = 20;

using namespace std::chrono;
using namespace openni;

AudioCloudSource& AudioCloudSource::get()
{
	static AudioCloudSource instance;
	instance.startLoop();
	return instance;
}

bool AudioCloudSource::hasNewData()
{
	std::lock_guard<std::mutex> recordLock(m_recordMutex);
	return m_hasNewData;
}

AudioCloudRecord AudioCloudSource::copyLatestCloud()
{
	std::lock_guard<std::mutex> recordLock(m_recordMutex);
	m_hasNewData = false;
	return m_lastCloud;
}


void AudioCloudSource::startLoop()
{
	m_CameraThread = std::thread([this]()
	{
		Camera depthCam;
		pcl::visualization::PCLVisualizer viz;
		viz.addPointCloud(boost::make_shared<PointCloud>());
		viz.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 1);

		while (true)
		{
			Timestamp captureTime = high_resolution_clock::now();
			auto spCamCloud = depthCam.captureFrame();
			auto spSegmentedPoints = segmentCloud(spCamCloud);

			viz.updatePointCloud(spSegmentedPoints);
			viz.spinOnce();
			//auto spWorldAudioCloud = transformToWorld(spSegmentedPoints, Eigen::Matrix3f()); // Grab rotation data from Eric for here

			// block for scope
			{
				std::lock_guard<std::mutex> recordLock(m_recordMutex);
				m_lastCloud = { spCamCloud, captureTime }; // move to segmented later
				m_hasNewData = true;
			}
		}
	});
	m_CameraThread.detach();
}


boost::shared_ptr<pcl::PointCloud<pcl::PointXYZRGB>> AudioCloudSource::segmentCloud(const boost::shared_ptr<PointCloud>& spCloud)
{
	auto spCloudTree = boost::make_shared<pcl::search::KdTree<pcl::PointXYZ>>();
	spCloudTree->setInputCloud(spCloud);

	std::vector<pcl::PointIndices> clusterIndices;
	pcl::EuclideanClusterExtraction<pcl::PointXYZ> eucCluster;
	eucCluster.setClusterTolerance(150); // 20cm
	eucCluster.setMinClusterSize(150);
	eucCluster.setMaxClusterSize(25000);
	
	eucCluster.setSearchMethod(spCloudTree);
	eucCluster.setInputCloud(spCloud);
	eucCluster.extract(clusterIndices);

	auto spColorCloud = boost::make_shared<pcl::PointCloud<pcl::PointXYZRGB>>();
	for (auto& indices1 : clusterIndices)
	{
		uint8_t r = 128 + (std::rand() % 128);
		uint8_t g = 128 + (std::rand() % 128);
		uint8_t b = 128 + (std::rand() % 128);

		for (int index : indices1.indices)
		{
			pcl::PointXYZRGB point(r, g, b);
			pcl::PointXYZ origPoint = (*spCloud)[index];
			point.x = origPoint.x;
			point.y = origPoint.y;
			point.z = origPoint.z;

			spColorCloud->push_back(point);
		}
	}

	return spColorCloud;
}


boost::shared_ptr<PointCloud> AudioCloudSource::transformToWorld(const boost::shared_ptr<PointCloud>& spCloud, const Eigen::Matrix3f& currentRotation)
{
	// TODO
	return boost::make_shared<PointCloud>();
}


Camera::Camera()
{
	static bool initialized = false;
	if (initialized)
		std::cerr << "Can't double construct camera" << std::endl;

	if (OpenNI::initialize() != STATUS_OK)
		std::cerr << "Can't initialize. Please debug me:" << OpenNI::getExtendedError() << std::endl;
	
	if (m_cameraDevice.open(ANY_DEVICE) != STATUS_OK)
		std::cerr << "Can't open device. Please debug me:" << OpenNI::getExtendedError() << std::endl;

	auto* pDepthInfo = m_cameraDevice.getSensorInfo(SENSOR_DEPTH);
	auto& supportedModes = pDepthInfo->getSupportedVideoModes();

	if (m_depthStream.create(m_cameraDevice, SENSOR_DEPTH) != STATUS_OK)
		std::cerr << "Can't open depth stream. Please debug me:" << OpenNI::getExtendedError() << std::endl;

	VideoMode vgaMode;
	vgaMode.setFps(30);
	vgaMode.setPixelFormat(PIXEL_FORMAT_DEPTH_1_MM);
	vgaMode.setResolution(640, 480);
	if (m_depthStream.setVideoMode(vgaMode) != STATUS_OK)
		std::cerr << "Can't set mode. Please debug me:" << OpenNI::getExtendedError() << std::endl;

	if (m_depthStream.start())
		std::cerr << "Can't start depth stream. Please debug me:" << OpenNI::getExtendedError() << std::endl;

	initialized = true;
}


boost::shared_ptr<PointCloud> Camera::captureFrame()
{
	VideoFrameRef frame;
	if (m_depthStream.readFrame(&frame) != STATUS_OK)
		std::cerr << "Can't read frame. Please debug me:" << OpenNI::getExtendedError() << std::endl;

	auto spWorldCloud =  boost::make_shared<PointCloud>();
	
	const auto* frameData = static_cast<const DepthPixel*>(frame.getData());
	for (int y = 0; y < frame.getHeight(); y++)
	{
		const DepthPixel* rowPtr = frameData + (y * frame.getWidth());
		for (int x = 0; x < frame.getWidth(); x++)
		{
			DepthPixel cameraDepth = rowPtr[x];
			if (cameraDepth == 0)
				continue;

			pcl::PointXYZ worldPt;
			CoordinateConverter::convertDepthToWorld(m_depthStream, x, y, cameraDepth, &worldPt.x, &worldPt.y, &worldPt.z);
			
			// change coordinate system
			worldPt.z *= -1;
			worldPt.x *= -1;

			spWorldCloud->push_back(worldPt);
		}	
	}

	// Reduce detail for faster processing
	pcl::VoxelGrid<pcl::PointXYZ> voxelGrid;
	voxelGrid.setInputCloud(spWorldCloud);
	voxelGrid.setLeafSize(50, 50, 50); // mm

	auto spReducedWorld = boost::make_shared<PointCloud>();
	voxelGrid.filter(*spReducedWorld);
	return spReducedWorld;
}



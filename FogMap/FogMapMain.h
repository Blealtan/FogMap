﻿#pragma once

#include "Common\StepTimer.h"
#include "Common\DeviceResources.h"
#include "Content\MainRenderer.h"
#include "Content\SampleFpsTextRenderer.h"

// 在屏幕上呈现 Direct2D 和 3D 内容。
namespace FogMap
{
	class FogMapMain : public DX::IDeviceNotify
	{
	public:
		FogMapMain(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		~FogMapMain();
		void CreateWindowSizeDependentResources();
		void Update();
		bool Render();

		// IDeviceNotify
		virtual void OnDeviceLost();
		virtual void OnDeviceRestored();

	private:
		// 缓存的设备资源指针。
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		// TODO: 替换为你自己的内容呈现器。
		std::unique_ptr<MainRenderer> m_sceneRenderer;
		std::unique_ptr<SampleFpsTextRenderer> m_fpsTextRenderer;

		// 渲染循环计时器。
		DX::StepTimer m_timer;
	};
}
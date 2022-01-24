#pragma once

#include "SeasonManager.h"

namespace FormSwap
{
	struct detail
	{
		static bool can_apply_snow_shader(RE::TESObjectREFR* a_ref, RE::NiAVObject* a_node)
		{
			const auto base = a_ref->GetBaseObject();
			if (!base || base->IsNot(RE::FormType::Activator, RE::FormType::Container, RE::FormType::Furniture, RE::FormType::MovableStatic, RE::FormType::Static) || base->IsMarker() || base->IsWater()) {
				return false;
			}

			if (!SeasonManager::GetSingleton()->IsSwapAllowed(base)) {
				return false;
			}

			if (!a_node || base->Is(RE::FormType::Activator) && a_node->HasAnimation()) {
				return false;
			}

			if (const auto parentCell = a_ref->GetParentCell(); parentCell) {
				const auto waterLevel = a_ref->GetSubmergedWaterLevel(a_ref->GetPositionZ(), parentCell);
				if (waterLevel >= 0.01f) {
					return false;
				}
			}

			if (const auto model = base->As<RE::TESModel>(); model) {
                if (const std::string path = model->model.c_str(); path.empty() || std::ranges::any_of(snowShaderBlackList, [&](const auto str) { return string::icontains(path, str); })) {
					return false;
				}
			} else {
				return false;
			}

			if (const auto stat = base->As<RE::TESObjectSTAT>(); stat) {
				const auto mat = stat->data.materialObj;
				if (mat && Cache::DataHolder::GetSingleton()->IsSnowShader(mat) || util::contains_textureset(stat, R"(Landscape\Snow)"sv)) {
					return false;
				}
			}

			return true;
		}

		static bool can_swap_static(const RE::TESObjectREFR* a_ref)
		{
			if (!SeasonManager::GetSingleton()->IsSwapAllowed()) {
				return false;
			}

			if (const auto parentCell = a_ref->GetParentCell(); parentCell) {
				const auto waterLevel = a_ref->GetSubmergedWaterLevel(a_ref->GetPositionZ(), parentCell);
				if (waterLevel >= 0.01f) {
					return false;
				}
			}

		    return true;
		}

	private:
		static inline std::array<std::string_view, 4> snowShaderBlackList = {
			R"(Effects\)",
			R"(Sky\)",
			"Marker"sv,
			"WetRocks"sv,
		};
	};

	struct Load3D
	{
		static RE::NiAVObject* thunk(RE::TESObjectREFR* a_ref, bool a_backgroundLoading)
		{
			const auto base = a_ref->GetBaseObject();
			const auto replaceBase = base && !base->IsDynamicForm() && detail::can_swap_static(a_ref) ? SeasonManager::GetSingleton()->GetSwapForm(base) : nullptr;

			if (replaceBase) {
				a_ref->SetObjectReference(replaceBase);
			}

			const auto node = func(a_ref, a_backgroundLoading);

			if (base && detail::can_apply_snow_shader(a_ref, node)) {
				const auto eid = util::get_editorID(base);
				auto& [init, projectedParams, projectedColor] = string::icontains(eid, "FarmHouse"sv) ? farmHouse : defaultObj;
				if (!init) {
					projectedColor = RE::TESForm::LookupByEditorID<RE::BGSMaterialObject>("SnowMaterialObject1P")->directionalData.singlePassColor;
					init = true;
				}
				if (node->SetProjectedUVData(projectedParams, projectedColor, true)) {
					if (const auto snowShaderData = RE::NiBooleanExtraData::Create("SOS_SNOW_SHADER", true); snowShaderData) {
						node->AddExtraData(snowShaderData);
					}
				}
			}

			return node;
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static inline constexpr std::size_t size = 0x6A;

	private:
		struct projectedUV
		{
			bool init{ false };
			RE::NiColorA projectedParams{};
			RE::NiColor projectedColor{};
		};

		static inline projectedUV defaultObj{
			.projectedParams = { 0.35f, 0.48f, 0.02f, 0.0f },
		};
		static inline projectedUV defaultObjLight{
			.projectedParams = { 0.225f, 0.82f, 0.000714f, 0.0f },
		};
		static inline projectedUV farmHouse{
			.projectedParams = { 0.35f, 0.5f, 0.02f, 0.0f },
		};
	};

	inline void Install()
	{
		stl::write_vfunc<RE::TESObjectREFR, Load3D>();
		logger::info("Installed form swapper"sv);
	}
}

#include <memory>

#include "I2cAccess.hpp"
#include "I2cToolsImpl.hpp"
#include "info/KernelInfo.hpp"
#include "PThresh.hpp"
#include "smbios/EntryPointEfi.hpp"
#include "smbios/SmBiosInfo.hpp"
#include "SystoolsdServices.hpp"
#include "Syscfg.hpp"
#include "TurboSettings.hpp"

using std::unique_ptr;

SystoolsdServices::SystoolsdServices() : Services()
{
    m_i2c = unique_ptr<I2cBase>(new I2cAccess(new I2cToolsImpl, 0));

    // Initialize SMBIOS
    m_smbios = unique_ptr<SmBiosInterface>(new SmBiosInfo);
    EntryPointEfi efi;
    efi.open();
    m_smbios->find_entry_point(&efi);
    m_smbios->parse();

    m_pthresh = unique_ptr<PThreshInterface>(new PThresh);
    m_turbo = unique_ptr<TurboSettingsInterface>(new TurboSettings);
    m_syscfg = unique_ptr<SyscfgInterface>(new Syscfg);
    m_kernel = unique_ptr<KernelInterface>(new KernelInfo);
}

SystoolsdServices::
SystoolsdServices(std::unique_ptr<I2cBase> i2c,
                  std::unique_ptr<SmBiosInterface> smbios,
                  std::unique_ptr<PThreshInterface> pthresh,
                  std::unique_ptr<TurboSettingsInterface> turbo,
                  std::unique_ptr<SyscfgInterface> syscfg,
                  std::unique_ptr<KernelInterface> kernel) : Services()
{
    m_i2c = std::move(i2c);
    m_smbios = std::move(smbios);
    m_pthresh = std::move(pthresh);
    m_turbo = std::move(turbo);
    m_syscfg = std::move(syscfg);
    m_kernel = std::move(kernel);
}

I2cBase *Services::get_i2c_srv() const
{
    return m_i2c.get();
}

SmBiosInterface *Services::get_smbios_srv() const
{
    return m_smbios.get();
}

PThreshInterface *Services::get_pthresh_srv() const
{
    return m_pthresh.get();
}

TurboSettingsInterface *Services::get_turbo_srv() const
{
    return m_turbo.get();
}

SyscfgInterface *Services::get_syscfg_srv() const
{
    return m_syscfg.get();
}

KernelInterface *Services::get_kernel_srv() const
{
    return m_kernel.get();;
}


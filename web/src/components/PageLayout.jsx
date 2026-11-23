import { machine } from '../services/ApiService.js';

export default function PageLayout({ variant = 'wide', children, className = '' }) {
  const widthClass = variant === 'narrow' ? 'mx-auto max-w-4xl w-full' : 'w-full';

  return (
    <div className={`${widthClass} flex flex-col gap-8 pb-20 lg:gap-10 ${className}`}>
      {machine.value.status.validationBuild && (
        <div role='alert' className='alert alert-warning'>
          VALIDATION FIRMWARE — simulated data only. No physical controller or machine outputs.
        </div>
      )}
      {children}
    </div>
  );
}

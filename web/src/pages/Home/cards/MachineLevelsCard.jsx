import PropTypes from 'prop-types';

export function MachineLevelsCard({
  hasTankLevel,
  tankLevelKnown,
  tankLevelOK,
  hasSteamLevel,
  steamLevelKnown,
  steamBoilerLow,
  inCard = false,
}) {
  const levels = [
    { label: 'Water tank', present: hasTankLevel, known: tankLevelKnown, low: !tankLevelOK },
    { label: 'Steam boiler', present: hasSteamLevel, known: steamLevelKnown, low: steamBoilerLow },
  ].filter(level => level.present);
  if (!levels.length) return null;

  return (
    <div
      className={
        inCard ? 'flex flex-col gap-2' : 'card bg-base-100 flex flex-col gap-2 rounded-xl p-3'
      }
    >
      <div className='text-base-content/50 text-[0.6rem] font-semibold tracking-wider uppercase'>
        Water Levels
      </div>
      <dl className='space-y-2 text-sm' aria-live='polite'>
        {levels.map(({ label, known, low }) => (
          <div key={label} className='flex items-center justify-between gap-3'>
            <dt>{label}</dt>
            <dd
              className={`text-xs font-semibold ${!known ? 'text-base-content/50' : low ? 'text-error' : 'text-success'}`}
            >
              {!known ? 'Unknown' : low ? 'Low' : 'OK'}
            </dd>
          </div>
        ))}
      </dl>
      <p className='text-base-content/40 text-[0.65rem]'>Level states received by the display.</p>
    </div>
  );
}

MachineLevelsCard.propTypes = {
  hasTankLevel: PropTypes.bool,
  tankLevelKnown: PropTypes.bool,
  tankLevelOK: PropTypes.bool,
  hasSteamLevel: PropTypes.bool,
  steamLevelKnown: PropTypes.bool,
  steamBoilerLow: PropTypes.bool,
  inCard: PropTypes.bool,
};
